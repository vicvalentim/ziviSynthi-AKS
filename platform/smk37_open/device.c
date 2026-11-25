/* ziviSynthiA panel and MIDI router for the SMK37 Open control task. */
#include "open_device.h"
#include "fm1_device.h"
#include "instrument.h"
#include "smk37_controls.h"
#include "smk37_hw.h"
#include "zivi_smk37.h"
#include "midi_clock.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "display.h"
#include "format.h"
#define snprintf lcd_format
#define APP_NAME "ziviSynthiA"
#define ZIVI_OPEN_PANEL
#define exp expf
#define exp2 exp2f
#define lround lroundf
#define sin sinf
#define cos cosf
#define sqrt sqrtf
#define log logf
#define fmax fmaxf
#define fmin fminf

enum { B_GLO, B_SEQ, B_ARP, B_REC, B_PLAY, B_HOME, B_SAVE, B_LFO, B_EDIT, B_FX, B_ENV };
enum { EN_K1, EN_K2, EN_K3, EN_K4, EN_PRESET, EN_SELECT, EN_ALGO, EN_MASTER };
enum { BTN_SHIFT, BTN_FADER_BANK, BTN_LEFT, BTN_RIGHT, BTN_OCT_MINUS, BTN_OCT_PLUS,
       BTN_ARP, BTN_REPEAT, BTN_SCALE, BTN_CHORD, BTN_GLOBE, BTN_BT, BTN_PATCH, BTN_PARA,
       BTN_FX, BTN_SEQ, BTN_PLAY, BTN_REC };
enum { PAD_COUNT = 16, FACTORY_FIRST_PAGE = 14, MIDI_QUEUE = 128 };
enum { VIEW_INSTRUMENT, VIEW_PRESETS, VIEW_BT, VIEW_MIDI };

static ZiviSmk37 zivi_device;
static SmkMidiClock midi_clock;
static uint32_t button_state, button_pressed, held, pressed;
static int encoders[8];
static unsigned octave_shift, preset_page, bank_cursor, view, bt_device, tx_channel, port_mode;
static unsigned clock_source = ZIVI_SMK_MIDI_PORTS;
static unsigned meter_left, meter_right, meter_bus, zivi_monitor_bus;
enum { TIMER_TICKS_PER_US = 24, AUDIO_DEADLINE_TICKS = (24000000ull * ZIVI_SMK_FRAMES + ZIVI_SMK_RATE - 1) / ZIVI_SMK_RATE };
static struct {
    uint32_t histogram[7], mean_q4, peak;
    unsigned minimum_reserve;
} audio_profile;
static uint32_t midi_overflows, midi_tx_failures;
static uint16_t midi_reset[ZIVI_SMK_MIDI_PORTS];
static bool ks_notes, clock_out, daw_mode, daw_playing, fader_bank;
static unsigned output_master = 127;
static uint8_t daw_values[2][8], output_note[37], midi_bank[ZIVI_SMK_MIDI_PORTS][16], faders[6];
static uint16_t daw_pads;
static struct { uint32_t packet, received; } midi_ring[MIDI_QUEUE];
static volatile unsigned midi_read, midi_write, midi_lost, audio_ready;
static unsigned input_ports;

static unsigned preset_start(unsigned page)
{
    return page ? FACTORY_FIRST_PAGE + (page - 1) * PAD_COUNT : 0;
}

static unsigned preset_pages(void)
{
    return 1 + (synth_factory_count() - FACTORY_FIRST_PAGE + PAD_COUNT - 1) / PAD_COUNT;
}

static unsigned page_size(unsigned page)
{
    unsigned left = synth_factory_count() - preset_start(page);
    unsigned size = page ? PAD_COUNT : FACTORY_FIRST_PAGE;
    return left < size ? left : size;
}

static void preset_reveal(unsigned index)
{
    preset_page = index < FACTORY_FIRST_PAGE ? 0 : 1 + (index - FACTORY_FIRST_PAGE) / PAD_COUNT;
    bank_cursor = index - preset_start(preset_page);
}

static int bound(int n, int lo, int hi) { return n < lo ? lo : n > hi ? hi : n; }
static uint32_t plat_ms(void) { return fm1_millis(); }
static uint32_t plat_pressed(void) { return button_pressed; }
static uint32_t plat_buttons(void) { return button_state; }
static int plat_enc(unsigned knob)
{
    int steps = encoders[knob];
    encoders[knob] = 0;
    return steps;
}

#define ZIVI_PRO_BOARD
#include "shared_panel.inc"

static void send_midi(unsigned status, unsigned data1, unsigned data2)
{
    uint8_t packet[3] = {(uint8_t)status, (uint8_t)data1, (uint8_t)data2};
    unsigned type = status & 0xf0;
    size_t bytes = status >= 0xf8 ? 1u : type == 0xc0 || type == 0xd0 ? 2u : 3u;
    unsigned ports = fm1_midi_ready_ports();
    if (port_mode) ports &= 1u << (port_mode - 1);
    for (unsigned port = 0; port < ZIVI_SMK_MIDI_PORTS; ++port) {
        if (!(ports & (1u << port))) continue;
        if (fm1_midi_send_port(port, packet, bytes) == -2) {
            ++midi_tx_failures;
            if (type == 0x80 || (type == 0x90 && data2 == 0) ||
                (type == 0xb0 && data2 == 0 && (data1 == 64 || data1 == 123)))
                midi_reset[port] |= (uint16_t)(1u << (status & 15u));
        }
    }
}

static void service_midi_tx(unsigned ports)
{
    for (unsigned port = 0; port < ZIVI_SMK_MIDI_PORTS; ++port) {
        unsigned waiting = midi_reset[port];
        if (!waiting || !(ports & (1u << port))) continue;
        unsigned channel = (unsigned)__builtin_ctz(waiting);
        uint8_t packet[] = {(uint8_t)(0xb0 | channel), 64, 0};
        if (fm1_midi_send_port(port, packet, sizeof packet)) continue;
        packet[1] = 123;
        if (!fm1_midi_send_port(port, packet, sizeof packet)) midi_reset[port] &= (uint16_t)~(1u << channel);
    }
}

static void note_off_all(void)
{
    for (unsigned k = 0; k < ZIVI_SMK_KEYS; ++k) {
        if (output_note[k] < 128) send_midi(0x80 | tx_channel, output_note[k], 0);
        output_note[k] = 255;
    }
    for (unsigned p = 0; p < PAD_COUNT; ++p)
        if (daw_pads & (1u << p)) send_midi(0x80 | tx_channel, 36 + p, 0);
    daw_pads = 0;
    send_midi(0xb0 | tx_channel, 64, 0);
    send_midi(0xb0 | tx_channel, 123, 0);
    if (daw_playing) send_midi(0xfc, 0, 0);
    daw_playing = false;
    app_panic();
    zivi_device.muted = true;
    meter_left = meter_right = meter_bus = zivi_monitor_bus = 0;
    unsigned irq = fm1_device_irq_save();
    zivi_device.read = zivi_device.write;
    fm1_device_irq_restore(irq);
}

static void select_preset(unsigned index)
{
    if (index < synth_factory_count()) {
        zivi_load_preset(4 + (int)index);
        if (daw_mode) zivi_device.muted = zivi_muted = true;
        preset_reveal(index);
    }
}

void zivi_open_midi(unsigned source, const uint8_t *message, size_t bytes)
{
    if (source >= ZIVI_SMK_MIDI_PORTS || !message || !bytes || bytes > 3 ||
        message[0] == 0xfe) return;
    uint32_t received = message[0] >= 0xf0 ? plat_ms() : 0;
    unsigned irq = fm1_device_irq_save();
    if (midi_write - midi_read == MIDI_QUEUE) midi_lost = 1;
    else {
        unsigned slot = midi_write % MIDI_QUEUE;
        midi_ring[slot].packet = message[0] |
            (bytes > 1 ? (uint32_t)message[1] << 8 : 0) |
            (bytes > 2 ? (uint32_t)message[2] << 16 : 0) | source << 24;
        midi_ring[slot].received = received;
        fm1_device_fence();
        ++midi_write;
    }
    fm1_device_irq_restore(irq);
    fm1_control_request();
}

static void service_midi(void)
{
    struct fm1_ble_status ble = {.state = FM1_BLE_OFF};
    fm1_ble_get_status(&ble);
    unsigned ready = fm1_midi_ready_ports();
    unsigned ports = ready & (1u << FM1_MIDI_USB);
    if (ble.state == FM1_BLE_READY) ports |= 1u << FM1_MIDI_BLE;
    unsigned detached = input_ports & ~ports;
    input_ports = ports;
    service_midi_tx(ready);
    if (midi_lost) {
        unsigned irq = fm1_device_irq_save();
        midi_read = midi_write;
        midi_lost = 0;
        fm1_device_irq_restore(irq);
        ++midi_overflows;
        detached = (1u << ZIVI_SMK_MIDI_PORTS) - 1u;
    }
    for (unsigned port = 0; port < ZIVI_SMK_MIDI_PORTS; ++port) {
        if (!(detached & (1u << port))) continue;
        zivi_smk_midi_port_disconnect(&zivi_device, port);
        memset(midi_bank[port], 0, sizeof midi_bank[port]);
        if (clock_source == port) {
            midi_clock_input(&midi_clock, &zivi.ks, 0xfc, 0, 0, plat_ms());
            midi_clock = (SmkMidiClock){.external = midi_clock.external};
            clock_source = ZIVI_SMK_MIDI_PORTS;
        }
    }
    for (unsigned n = 0; n < 24 && midi_read != midi_write; ++n) {
        fm1_device_fence();
        unsigned slot = midi_read % MIDI_QUEUE;
        uint32_t packet = midi_ring[slot].packet, received = midi_ring[slot].received;
        fm1_device_fence();
        ++midi_read;
        unsigned status = packet & 255, a = (packet >> 8) & 127, b = (packet >> 16) & 127;
        unsigned type = status & 0xf0, channel = status & 15, source = packet >> 24;
        if (detached & (1u << source)) continue;
        if (status == 0xff) {
            zivi_smk_midi_port_disconnect(&zivi_device, source);
            memset(midi_bank[source], 0, sizeof midi_bank[source]);
            if (clock_source == source) {
                midi_clock_input(&midi_clock, &zivi.ks, 0xfc, 0, 0, received);
                midi_clock = (SmkMidiClock){.external = midi_clock.external};
                clock_source = ZIVI_SMK_MIDI_PORTS;
            }
            continue;
        }
        if (status >= 0xf0) {
            if (midi_clock.external && clock_source == ZIVI_SMK_MIDI_PORTS &&
                (status == 0xf8 || status == 0xfa || status == 0xfb || status == 0xf2))
                clock_source = source;
            if (clock_source == source) midi_clock_input(&midi_clock, &zivi.ks, status, a, b, received);
            continue;
        }
        if (zivi.midi_channel && zivi.midi_channel != channel + 1) continue;
        if (type == 0xb0 && a == 0) midi_bank[source][channel] = (uint8_t)b;
        else if (type == 0xc0) {
            unsigned bank = midi_bank[source][channel];
            if (!bank) select_preset(a);
            else if (bank <= preset_pages() && a < page_size(bank - 1))
                select_preset(preset_start(bank - 1) + a);
        } else if (type == 0xb0 && a == 74) {
            synth_patch_controls(&zivi, zivi.mix, zivi.lfo, 25.0f * exp2f(8.6f * b / 127));
            zivi_dirty = 1;
        } else if (type == 0xb0 && (a == 71 || a == 91 || a == 10)) {
            synth_module_control(&zivi, a == 71 ? M4_RESPONSE : a == 91 ? M4_REVERB_MIX : M4_OUT1_PAN,
                                 a == 10 ? 2.0f * b / 127 - 1 : b / 127.0f);
            zivi_dirty = 1;
        } else if (zivi_smk_midi_port(&zivi_device, source, status, a, b)) {
            if (type == 0x90 && b && !daw_mode) zivi_device.muted = zivi_muted = false;
            if (type == 0xb0 && (a == 1 || a == 7)) zivi_dirty = 1;
        }
    }
}

static unsigned button_mask(uint32_t physical)
{
    static const uint8_t map[18] = {B_ENV, 255, B_HOME, B_SAVE, 255, 255, B_ARP,
        B_LFO, 255, 255, B_GLO, 255, 255, B_EDIT, B_FX, B_SEQ, B_PLAY, B_REC};
    unsigned mapped = 0;
    for (unsigned i = 0; i < 18; ++i)
        if (map[i] != 255 && (physical & (1u << i))) mapped |= 1u << map[i];
    return mapped;
}

static void pad_press(unsigned pad, unsigned speed)
{
    if (daw_mode) {
        if (speed) daw_pads |= (uint16_t)(1u << pad);
        else daw_pads &= (uint16_t)~(1u << pad);
        send_midi((speed ? 0x90u : 0x80u) | tx_channel, 36 + pad, speed);
    } else if (speed) {
        if (view == VIEW_PRESETS && pad < page_size(preset_page))
            select_preset(preset_start(preset_page) + pad);
        else if (view == VIEW_INSTRUMENT && !zivi_module_page && !zivi_ks_page) {
            if (zivi_smk_pad(&zivi_device, zivi_row, zivi_col, pad, zivi_pin_type)) {
                zivi_row = (zivi_row & ~3u) + pad / MATRIX_TILE;
                zivi_col = (zivi_col & ~3u) + pad % MATRIX_TILE;
                zivi_preset = -1;
            }
            zivi_zoom = true;
            zivi_dirty = 1;
        }
    }
}

void smk37_control_event(unsigned kind, unsigned id, int value)
{
    if (kind == SMK37_BUTTON && id < 18) {
        unsigned bit = 1u << id;
        if (value) {
            if (!(held & bit)) pressed |= bit;
            held |= bit;
        } else held &= ~bit;
    } else if (kind == SMK37_ENCODER && id < 8) {
        encoders[id] = bound(encoders[id] + value, -127, 127);
    } else if (kind == SMK37_KEY && id < ZIVI_SMK_KEYS) {
        unsigned note = value ? (unsigned)(12 * octave_shift + id) : output_note[id];
        if (!daw_mode) {
            zivi_smk_key(&zivi_device, id, (unsigned)value, (int)octave_shift - 4);
            if (value) zivi_device.muted = zivi_muted = false;
        }
        if (note <= 127) send_midi((value ? 0x90u : 0x80u) | tx_channel, note, (unsigned)value);
        output_note[id] = value ? (uint8_t)note : 255;
    } else if (kind == SMK37_PAD && id < 16) {
        pad_press(id, (unsigned)value);
    } else if (kind == SMK37_PRESSURE && id < 16 && daw_mode && (daw_pads & (1u << id))) {
        send_midi(0xa0 | tx_channel, 36 + id, (unsigned)value);
    } else if (kind == SMK37_PEDAL) {
        send_midi(0xb0 | tx_channel, 64, value ? 127 : 0);
        if (value && !daw_mode) {
            zivi_device.muted = zivi_muted = false;
            synth_manual_attack(&zivi);
        }
    } else if (kind == SMK37_FADER && id < 6) {
        unsigned previous = faders[id];
        if (previous != 255 && abs(value - (int)previous) < (id < 4 ? 2 : 1)) return;
        faders[id] = (uint8_t)value;
        unsigned channel = zivi.midi_channel ? zivi.midi_channel - 1 : 0;
        if (id >= 4) {
            unsigned status = id == 4 ? 0xe0 : 0xb0, first = id == 4 ? 0 : 1;
            send_midi(status | tx_channel, first, (unsigned)value);
            if (!daw_mode) zivi_smk_midi(&zivi_device, status | channel, first, (unsigned)value);
        } else if (daw_mode) {
            send_midi(0xb0 | tx_channel, (fader_bank ? 76u : 70u) + id, (unsigned)value);
        } else if (previous != 255) {
            float fraction = value / 127.0f;
            if (id == 0) output_master = (unsigned)value;
            if (id == 1) synth_controls(&zivi, zivi.tune, fraction, zivi.level);
            if (id == 2) synth_patch_controls(&zivi, zivi.mix, zivi.lfo, 55.0f * exp2f(8.0f * fraction));
            if (id == 3) synth_module_control(&zivi, M4_REVERB_MIX, fraction);
            if (id != 0) zivi_preset = -1;
            zivi_dirty = 1;
        }
    }
}

void smk37_controls_panic(void) { note_off_all(); }

static unsigned audio_reserve(void)
{
    unsigned read = zivi_device.read, write = zivi_device.write;
    return (write + ZIVI_SMK_SLOTS - read) % ZIVI_SMK_SLOTS;
}

static void profile_render(uint32_t ticks)
{
    if (ticks > audio_profile.peak) audio_profile.peak = ticks;
    unsigned bin = ticks >= AUDIO_DEADLINE_TICKS ? 6 : ticks / (500 * TIMER_TICKS_PER_US);
    ++audio_profile.histogram[bin];
    /* EWMA with 1/16 weight on the last block. Cap an outlier only for
     * the mean, never for the maximum or the deadline count. */
    if (ticks > 24000000u) ticks = 24000000u;
    audio_profile.mean_q4 += ticks - (audio_profile.mean_q4 >> 4);
}

static unsigned profile_p95_us(void)
{
    uint32_t count = 0, accumulated = 0;
    for (unsigned bin = 0; bin < 7; ++bin) count += audio_profile.histogram[bin];
    if (!count) return 0;
    for (unsigned bin = 0; bin < 6; ++bin) {
        accumulated += audio_profile.histogram[bin];
        if (accumulated >= count - count / 20)
            return bin < 5 ? (bin + 1) * 500 : (AUDIO_DEADLINE_TICKS + 23) / 24;
    }
    return (audio_profile.peak + TIMER_TICKS_PER_US - 1) / TIMER_TICKS_PER_US;
}

static void reset_profile(void)
{
    memset(&audio_profile, 0, sizeof audio_profile);
    audio_profile.minimum_reserve = audio_reserve();
}

static void fill_audio(void)
{
    bool visible = view == VIEW_INSTRUMENT && !daw_mode;
    bool meter = visible && !zivi_module_page && !zivi_ks_page;
    bool trace = visible && zivi_module_page && zivi_scope && !zivi_scope_frozen;
    for (unsigned i = 0; i < ZIVI_SMK_SLOTS - 1; ++i) {
        unsigned at = zivi_device.write;
        uint32_t start = fm1_device_ticks();
        if (!zivi_smk_fill(&zivi_device)) break;
        if (zivi_device.muted) continue;
        profile_render(fm1_device_ticks() - start);
        if (meter) {
            unsigned bus = (unsigned)(zivi.meter_peak * 53.0f);
            if (bus > meter_bus) meter_bus = bus;
            for (unsigned f = 0; f < ZIVI_SMK_FRAMES; f += TRACE_DECIMATION) {
                unsigned left = (unsigned)abs(zivi_device.ring[at][2 * f]) >> 17;
                unsigned right = (unsigned)abs(zivi_device.ring[at][2 * f + 1]) >> 17;
                if (left > meter_left) meter_left = left;
                if (right > meter_right) meter_right = right;
            }
        } else if (trace) {
            for (unsigned f = 0; f < ZIVI_SMK_FRAMES; f += TRACE_DECIMATION) {
                int32_t sample = zivi_device.ring[at][2 * f] >> 9;
                zivi_trace[zivi_trace_head] = (int16_t)(sample < -16384 ? -16384 : sample > 16383 ? 16383 : sample);
                zivi_trace_head = (zivi_trace_head + 1) % TRACE_POINTS;
                if (zivi_trace_count < TRACE_POINTS) ++zivi_trace_count;
            }
        }
    }
}

FM1_AUDIO_CODE int zivi_open_audio(int32_t *out, unsigned frames)
{
    if (frames != FM1_BLOCK_FRAMES) return -1;
    if (audio_ready) (void)zivi_smk_irq_read(&zivi_device, out);
    else {
        volatile int32_t *target = out;
        for (unsigned i = 0; i < FM1_BLOCK_WORDS; ++i) target[i] = 0;
    }
    return 0;
}

static void draw_presets(void)
{
    char label[24];
    snprintf(label, sizeof label, "BANK %u/%u", preset_page + 1, preset_pages());
    zivi_header(label);
    static const char *const banks[] = {"ESTUDIO / LAB", "PERFORMANCE", "TEXTURAS", "SEQUENCIAS KS"};
    cv_text(10, 38, &FONT_XS, banks[preset_page], INK);
    unsigned start = preset_start(preset_page);
    unsigned count = page_size(preset_page);
    for (unsigned i = 0; i < count; ++i) {
        int x = 8 + (int)(i / 8) * 114, y = 54 + (int)(i % 8) * 16;
        unsigned index = start + i;
        bool active = zivi_preset == (int)index + 4;
        bool cursor = bank_cursor == i;
        cv_rect(x, y, 110, 14, active ? DARK_BLUE : cursor ? BLACK : SILVER);
        snprintf(label, sizeof label, "%02u", i + 1);
        cv_text(x + 3, y + 4, &FONT_XS, label, active || cursor ? AMBER : INK);
        const char *name = synth_factory_display(index);
        unsigned visible = 0;
        while (visible < 15 && name[visible]) ++visible;
        memcpy(label, name, visible);
        label[visible] = 0;
        cv_text(x + 18, y + 4, &FONT_XS, label, active || cursor ? WHITE : INK);
    }
    if (bank_cursor < count)
        cv_text(10, 191, &FONT_S, synth_factory_display(start + bank_cursor), INK);
    zivi_footer("PADS LOAD   K5 / PARA   BANK < >");
    cv_blit(0, 0);
}

static void draw_controls(void)
{
    char line[40];
    if (view == VIEW_BT) {
        static const char *const states[] = {"OFF", "IDLE", "ADVERTISING", "SCANNING", "CONNECTING",
            "DISCOVERING", "CONNECTED", "READY", "DISCONNECTING"};
        struct fm1_ble_status ble = {.state = FM1_BLE_OFF};
        struct fm1_bluetooth_status radio = {.features = 0};
        fm1_ble_get_status(&ble);
        fm1_bluetooth_get_status(&radio);
        zivi_header("BLUETOOTH");
        cv_rect(8, 36, 224, 40, BLACK);
        const char *state = ble.state < sizeof states / sizeof states[0] ? states[ble.state] : "?";
        const char *audio = radio.audio_playing ? "PLAY" : radio.audio_connected ? "READY" : "WAIT";
        snprintf(line, sizeof line, "%s   MIDI %s", ble.role == FM1_BLE_CENTRAL ? "HOST" : "DEVICE", state);
        cv_text(14, 43, &FONT_XS, line, WHITE);
        snprintf(line, sizeof line, "AUDIO %s   %s", radio.features & FM1_BT_AUDIO ? "ON" : "OFF", audio);
        cv_text(14, 60, &FONT_XS, line, AMBER);
        for (unsigned i = 0; i < ble.count && i < FM1_BLE_MAX_DEVICES; ++i) {
            struct fm1_ble_device dev;
            if (!fm1_ble_get_device(i, &dev)) {
                int y = 82 + (int)i * 12;
                if (i == bt_device) cv_rect(8, y - 2, 224, 11, SILVER);
                dev.name[sizeof dev.name - 1] = 0;
                snprintf(line, sizeof line, "%02u %s", i + 1, dev.name);
                cv_text(13, y, &FONT_XS, line, INK);
            }
        }
        if (ble.error) {
            snprintf(line, sizeof line, "BLE ERROR %u", (unsigned)ble.error);
            cv_text(10, 184, &FONT_XS, line, RED);
        }
        cv_text(10, 201, &FONT_XS, "K1 RADIO K2 AUDIO K3 ROLE K6 PICK", INK);
        zivi_footer("PLAY SCAN  PARA CONNECT  < DROP");
    } else if (view == VIEW_MIDI) {
        struct fm1_audio_stats stats;
        fm1_audio_stats_read(&stats);
        zivi_header("MIDI / AUDIO");
        snprintf(line, sizeof line, "K1 TX %u  K2 RX %u", tx_channel + 1, (unsigned)zivi.midi_channel);
        cv_text(10, 42, &FONT_S, line, INK);
        snprintf(line, sizeof line, "K3 OUT %s  K8 VOL %u", port_mode == 1 ? "TRS" : port_mode == 2 ? "USB" : port_mode == 3 ? "BLE" : "ALL", output_master);
        cv_text(10, 59, &FONT_S, line, INK);
        snprintf(line, sizeof line, "K5 SYNC %s  K6 KS %s", midi_clock.external ? midi_clock.locked ? "LOCK" : "EXT" : "INT",
                 ks_notes ? "ON" : "OFF");
        cv_text(10, 76, &FONT_S, line, INK);
        snprintf(line, sizeof line, "K7 CLOCK %s   RX 0 OMNI", clock_out ? "ON" : "OFF");
        cv_text(10, 93, &FONT_S, line, INK);
        cv_rect(8, 112, 224, 99, BLACK);
        snprintf(line, sizeof line, "FIFO %u/%u  MIN %u  MISS %u", audio_reserve(),
                 ZIVI_SMK_SLOTS - 1u, audio_profile.minimum_reserve, zivi_device.underruns);
        cv_text(14, 119, &FONT_XS, line, AMBER);
        snprintf(line, sizeof line, "DSP US  AVG %u  P95<=%u", audio_profile.mean_q4 / (16 * TIMER_TICKS_PER_US),
                 profile_p95_us());
        cv_text(14, 133, &FONT_XS, line, WHITE);
        snprintf(line, sizeof line, "MAX %u  LATE %u", audio_profile.peak / TIMER_TICKS_PER_US,
                 audio_profile.histogram[6]);
        cv_text(14, 147, &FONT_XS, line, WHITE);
        snprintf(line, sizeof line, "DMA %u  LCD %u", (unsigned)stats.overruns, screen_faults);
        cv_text(14, 161, &FONT_XS, line, WHITE);
        snprintf(line, sizeof line, "MIDI RX %u  TX %u", (unsigned)midi_overflows, (unsigned)midi_tx_failures);
        cv_text(14, 175, &FONT_XS, line, WHITE);
        cv_text(14, 194, &FONT_XS, "PRO OPEN " ZIVI_VERSION, AMBER);
        zivi_footer("SCALE RETURN  SHIFT+PARA RESET DSP");
    } else {
        zivi_header("CONTROLLER");
        cv_rect(8, 36, 224, 50, BLACK);
        cv_text(14, 44, &FONT_XS, "MIDI OUTPUT", AMBER);
        snprintf(line, sizeof line, "TX %02u / FADERS %c", tx_channel + 1, fader_bank ? 'B' : 'A');
        cv_text(14, 63, &FONT_B, line, WHITE);
        cv_text(10, 103, &FONT_S, "PADS 36-51 / POLY PRESSURE", INK);
        cv_text(10, 123, &FONT_S, "K1-8 CC16-23 / CC24-31", INK);
        cv_text(10, 143, &FONT_S, daw_playing ? "TRANSPORT PLAY" : "TRANSPORT STOP", INK);
        zivi_footer("GLOBE + PATCH RETURN");
    }
    cv_blit(0, 0);
}

static void service_buttons(void)
{
    unsigned buttons = pressed;
    pressed = 0;
    unsigned previous_view = view;
    unsigned combo = (1u << BTN_GLOBE) | (1u << BTN_PATCH);
    if ((held & combo) == combo && (buttons & combo)) {
        note_off_all();
        daw_mode = !daw_mode;
        view = VIEW_INSTRUMENT;
        buttons &= ~combo;
        zivi_dirty = 1;
    }
    if (buttons & (1u << BTN_BT)) view = view == VIEW_BT ? VIEW_INSTRUMENT : VIEW_BT;
    if (buttons & (1u << BTN_SCALE)) view = view == VIEW_MIDI ? VIEW_INSTRUMENT : VIEW_MIDI;
    if (!daw_mode && (buttons & (1u << BTN_PATCH)))
        view = view == VIEW_PRESETS ? VIEW_INSTRUMENT : VIEW_PRESETS;
    if (!daw_mode && (buttons & ((1u << BTN_SEQ) | (1u << BTN_GLOBE))))
        view = VIEW_INSTRUMENT;
    if (view != previous_view) {
        zivi_dirty = 1;
        band_cursor = 0;
    }
    if (buttons & (1u << BTN_CHORD)) note_off_all();
    if (buttons & (1u << BTN_OCT_MINUS)) octave_shift = octave_shift ? octave_shift - 1 : 0;
    if (buttons & (1u << BTN_OCT_PLUS)) octave_shift = octave_shift < 7 ? octave_shift + 1 : 7;

    if (view == VIEW_BT) {
        unsigned flags = groove_settings_radio_flags(), role = groove_settings_role_value();
        if (plat_enc(0) % 2) flags ^= GROOVE_RADIO_ON;
        if (plat_enc(1) % 2) flags ^= GROOVE_RADIO_AUDIO;
        if (plat_enc(2) % 2) role ^= 1u;
        if (flags != groove_settings_radio_flags() || role != groove_settings_role_value()) {
            (void)groove_settings_radio(flags, role);
            zivi_dirty = 1;
        }
        struct fm1_ble_status ble = {.state = FM1_BLE_OFF};
        fm1_ble_get_status(&ble);
        unsigned peer = (unsigned)bound((int)bt_device + plat_enc(EN_SELECT), 0, ble.count ? ble.count - 1 : 0);
        if (bt_device != peer) zivi_dirty = 1;
        bt_device = peer;
        if (buttons & (1u << BTN_PLAY)) (void)fm1_ble_scan();
        if ((buttons & (1u << BTN_PARA)) && bt_device < ble.count) (void)fm1_ble_connect(bt_device);
        if (buttons & (1u << BTN_LEFT)) (void)fm1_ble_disconnect();
        if ((buttons & (1u << BTN_REC)) && (held & (1u << BTN_SHIFT))) (void)fm1_ble_forget_bonds();
    } else if (view == VIEW_MIDI) {
        if ((buttons & (1u << BTN_PARA)) && (held & (1u << BTN_SHIFT))) {
            reset_profile();
            zivi_dirty = 1;
        }
        unsigned tx = (unsigned)bound((int)tx_channel + plat_enc(0), 0, 15);
        unsigned ports = (unsigned)bound((int)port_mode + plat_enc(2), 0, 3);
        if (tx != tx_channel || ports != port_mode) note_off_all();
        tx_channel = tx;
        port_mode = ports;
        int rx = plat_enc(1);
        if (rx) {
            zivi_smk_midi_disconnect(&zivi_device);
            zivi.midi_channel = (uint8_t)bound((int)zivi.midi_channel + rx, 0, 16);
        }
        int clock = plat_enc(EN_PRESET);
        if (clock && midi_clock.external != (clock > 0)) {
            midi_clock = (SmkMidiClock){.external = clock > 0};
            clock_source = ZIVI_SMK_MIDI_PORTS;
        }
        int notes = plat_enc(EN_SELECT), ticks = plat_enc(EN_ALGO);
        if (notes) ks_notes = notes > 0;
        if (ticks) clock_out = ticks > 0;
        if (rx || clock || notes || ticks) zivi_dirty = 1;
    } else if (daw_mode) {
        if (buttons & (1u << BTN_FADER_BANK)) fader_bank = !fader_bank;
        unsigned bank = (held & (1u << BTN_SHIFT)) != 0;
        for (unsigned k = 0; k < 8; ++k) {
            int turn = plat_enc(k);
            if (turn) {
                unsigned value = (unsigned)bound((int)daw_values[bank][k] + turn, 0, 127);
                if (value != daw_values[bank][k]) send_midi(0xb0 | tx_channel, 16 + bank * 8 + k, value);
                daw_values[bank][k] = (uint8_t)value;
            }
        }
        if (buttons & (1u << BTN_PLAY)) {
            daw_playing = !daw_playing;
            send_midi(daw_playing ? 0xfa : 0xfc, 0, 0);
        }
        if (buttons & (1u << BTN_REC)) send_midi(0xb0 | tx_channel, 115, 127);
        if (buttons & (1u << BTN_LEFT)) send_midi(0xb0 | tx_channel, 116, 127);
        if (buttons & (1u << BTN_RIGHT)) send_midi(0xb0 | tx_channel, 117, 127);
        if (buttons & (1u << BTN_SEQ)) send_midi(0xb0 | tx_channel, 118, 127);
        if (buttons) zivi_dirty = 1;
        return;
    } else if (view == VIEW_PRESETS) {
        unsigned pages = preset_pages(), page = preset_page;
        if (buttons & (1u << BTN_LEFT)) page = (page + pages - 1) % pages;
        if (buttons & (1u << BTN_RIGHT)) page = (page + 1) % pages;
        int turn = plat_enc(EN_PRESET);
        if (page != preset_page) {
            preset_page = page;
            bank_cursor = 0;
        }
        bank_cursor = (unsigned)bound((int)bank_cursor + turn, 0, (int)page_size(page) - 1);
        if (buttons & (1u << BTN_PARA)) select_preset(preset_start(page) + bank_cursor);
        if (buttons || turn) zivi_dirty = 1;
    } else {
        int previous = zivi_preset;
        button_state = button_mask(held);
        button_pressed = button_mask(buttons);
        app_input_only();
        if (zivi.modules.manual_attack || zivi.ks.random_held || zivi.ks.mode == KS_PLAYING)
            zivi_device.muted = zivi_muted = false;
        if (previous != zivi_preset && zivi_preset >= 4) preset_reveal((unsigned)zivi_preset - 4);
    }
    unsigned master = (unsigned)bound((int)output_master + plat_enc(EN_MASTER) * 2, 0, 127);
    if (master != output_master) zivi_dirty = 1;
    output_master = master;
    (void)groove_master_update((output_master * 1023u + 63u) / 127u);
    memset(encoders, 0, sizeof encoders);
}

static void service_transport(uint32_t now)
{
    static uint32_t next, prior;
    static bool clock_running;
    static unsigned note = 255;
    bool playing = zivi.ks.mode == KS_PLAYING && !daw_mode;
    bool send_clock = playing && clock_out && !midi_clock.external;
    if (playing) zivi_device.muted = zivi_muted = false;
    if (send_clock != clock_running) {
        send_midi(send_clock ? 0xfa : 0xfc, 0, 0);
        clock_running = send_clock;
        next = 0;
        prior = now;
    }
    if (send_clock) {
        unsigned elapsed = now - prior;
        prior = now;
        if (elapsed > 250u) next = 0;
        else next += elapsed * 6u * zivi.ks.speed;
        for (unsigned i = 0; i < 3 && next >= 12500u; ++i, next -= 12500u)
            send_midi(0xf8, 0, 0);
        next %= 12500u;
    }
    unsigned wanted = playing && ks_notes && zivi.ks.velocity ?
        (unsigned)bound(zivi.ks.note + zivi.ks.transpose, 0, 127) : 255;
    if (wanted != note) {
        if (note < 128) send_midi(0x80 | tx_channel, note, 0);
        if (wanted < 128) send_midi(0x90 | tx_channel, wanted, zivi.ks.velocity);
        note = wanted;
    }
}

static void panel_leds(void)
{
    for (unsigned p = 0; p < PAD_COUNT; ++p) {
        unsigned rgb = 0;
        if (daw_mode) rgb = daw_pads & (1u << p) ? 0x305020 : 0x101820;
        else if (view == VIEW_PRESETS && p < page_size(preset_page))
            rgb = (int)(4 + preset_start(preset_page) + p) == zivi_preset ? 0x404020 : 0x102828;
        else if (view == VIEW_INSTRUMENT && !zivi_ks_page && !zivi_module_page) {
            unsigned r = (zivi_row & ~3u) + p / 4, c = (zivi_col & ~3u) + p % 4;
            rgb = zivi.matrix[r][c] == PIN_FULL ? 0x083000 : zivi.matrix[r][c] == PIN_HALF ? 0x302000 : 0;
            if (zivi_row == r && zivi_col == c) rgb |= 0x300030;
        }
        smk37_rgb_set(p, rgb);
    }
    smk37_rgb_service();
}

void groove_screen_init(void)
{
    app_init();
    memset(output_note, 255, sizeof output_note);
    memset(faders, 255, sizeof faders);
    memset(daw_values, 64, sizeof daw_values);
    octave_shift = 4;
    fm1_controls_init();
    fm1_screen_init();
    (void)groove_master_update(1023);
    fill_audio();
    reset_profile();
    fm1_device_fence();
    audio_ready = 1;
}

void groove_screen_control(void)
{
    static uint32_t last_frame, last_overlay, last_meter, last_led;
    unsigned reserve = audio_reserve();
    if (reserve < audio_profile.minimum_reserve) audio_profile.minimum_reserve = reserve;
    /* Apply already received notes and patch changes before publishing new PCM,
     * including the first refill after an empty queue. */
    smk37_scan_read();
    service_midi();
    service_buttons();
    uint32_t now = plat_ms();
    midi_clock_timeout(&midi_clock, &zivi.ks, now);
    if (!midi_clock.seen && !midi_clock.pending_start) clock_source = ZIVI_SMK_MIDI_PORTS;
    service_transport(now);
    fill_audio();
    if (now - last_meter >= 50) {
        last_meter = now;
        unsigned left = zivi_peak > 5 ? zivi_peak - 5 : 0;
        unsigned right = zivi_monitor > 5 ? zivi_monitor - 5 : 0;
        unsigned bus = zivi_monitor_bus > 5 ? zivi_monitor_bus - 5 : 0;
        if (meter_bus > bus) bus = meter_bus;
        if (meter_left > left) left = meter_left;
        if (meter_right > right) right = meter_right;
        if (view == VIEW_INSTRUMENT && !daw_mode && !zivi_module_page && !zivi_ks_page &&
            (left != zivi_peak || right != zivi_monitor || bus != zivi_monitor_bus)) zivi_dirty = 1;
        zivi_peak = left;
        zivi_monitor = right;
        zivi_monitor_bus = bus;
        meter_left = meter_right = meter_bus = 0;
    }

    /* The DMA buffer must be released before redrawing or touching SPI for LEDs. */
    if (!screen_idle() || audio_reserve() < 3) return;
    if (now - last_led >= 40) {
        last_led = now;
        panel_leds();
    }
    if (now - last_frame >= 30) {
        last_frame = now;
        if (view == VIEW_INSTRUMENT && !daw_mode) {
            zivi_update_frame();
        } else if (zivi_dirty || ((view == VIEW_BT || view == VIEW_MIDI) && now - last_overlay >= 200)) {
            if (view == VIEW_PRESETS) draw_presets();
            else draw_controls();
            zivi_dirty = 0;
            last_overlay = now;
        }
    }
    if (audio_reserve() >= 3) screen_send_stripe();
}

void groove_screen_mmc(unsigned command)
{
    if (command == 1 || command == 9) ks_stop(&zivi.ks);
    else if (command == 2 || command == 3) (void)ks_play(&zivi.ks);
    else if (command == 6) synth_ks_record(&zivi);
}
