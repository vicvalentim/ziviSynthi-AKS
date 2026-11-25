/* ziviSynthiA application with the SLOOP 2.8.5 SMK-37 Pro hardware layer.
 * The included HAL/USB/LCD files retain their upstream copyright notices. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "zivi_smk37.h"
#include "midi_clock.h"
#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_irq.h"
#include "fm1_guard.h"
#include "smk_input.h"
#include "fm1_timer.h"
#include "fm1_audio.h"
#include "fm1_lcd_hw.h"
#define FL_RANGE_OK(off, n) FL_IN(off, n, FL_OTA_LO, FL_OTA_HI)
#include "fm1_flash.h"
#include "bootguard.h"
#include "smk_padlight.h"
#include "smk_wheel.h"
#include "smk_pedal.h"
#include "smk_uart_tx.h"
#include "smk_midi_out.h"

#define FELUCCA_OTA 1
#define FELUCCA_UAC 1
#define HALF_FRAMES ZIVI_SMK_FRAMES
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#include "usb_zivi.c"
_Static_assert((unsigned)USB_PCM_BLOCK == (unsigned)ZIVI_SMK_FRAMES, "USB and codec block size");
#include "lcd.c"
#include "display.h"
#include "format.h"
#define snprintf lcd_format

#define APP_NAME "ziviSynthiA SMK"
enum { B_GLO, B_SEQ, B_ARP, B_REC, B_PLAY, B_HOME, B_SAVE, B_LFO, B_EDIT, B_FX, B_ENV };
enum { EN_K1, EN_K2, EN_K3, EN_K4, EN_PRESET, EN_SELECT, EN_ALGO, EN_MASTER };
enum { BTN_BANK = 0, BTN_FADER_BANK = 1, BTN_LEFT = 2, BTN_RIGHT = 3,
       BTN_ARP = 6, BTN_REPEAT = 7, BTN_DIAGNOSTICS = 8, BTN_PANIC = 9,
       BTN_GLOBE = 10, BTN_PATCH = 12, BTN_PARA = 13, BTN_FX = 14,
       BTN_SEQ = 15, BTN_PLAY = 16, BTN_REC = 17 };

static ZiviSmk37 zivi_device;
static uint32_t button_state, button_pressed;
static uint32_t lcd_ready_tick;
static volatile uint32_t uptime_ms;
static uint32_t render_max_us;
static volatile uint32_t audio_irqs;
static volatile bool audio_busy;
static int octave;
enum { PAD_COUNT = 16, FACTORY_FIRST_PAGE = 14 };
static unsigned preset_page, midi_channel_out, midi_bank[16];
static uint8_t external_keys[ZIVI_SMK_KEYS], daw_knobs[16], daw_pressure[PAD_COUNT];
static unsigned daw_pads;
static bool daw_mode, daw_knob_bank, daw_fader_bank, daw_playing;
static bool thru_trs, ks_notes_out, clock_out;
static SmkMidiClock midi_clock;
static bool diagnostics, recovery_mode, update_armed, update_hold;
static uint32_t update_until;
static int update_error;
static uint64_t key_state;
static uint8_t fader_last[4];
static smk_pw_t pitch_wheel;
static smk_mw_t mod_wheel;
static smk_pedal_t pedal;
static bool pedal_down;
static int last_pitch = 8192, last_mod;
static bootguard_t bootguard __attribute__((section(".noinit.zivi_guard")));
static int32_t dma_words[2 * ZIVI_SMK_WORDS] __attribute__((aligned(4)));
/* The XIP-to-SRAM distance exceeds the direct call displacement. */
static bool (*volatile read_audio)(ZiviSmk37 *, volatile int32_t *) = zivi_smk_irq_read;

static unsigned preset_page_first(unsigned page)
{
    return page ? FACTORY_FIRST_PAGE + (page - 1) * PAD_COUNT : 0;
}

static unsigned preset_pages(void)
{
    unsigned count = synth_factory_count();
    return count <= FACTORY_FIRST_PAGE ? 1 : 1 + (count - FACTORY_FIRST_PAGE + PAD_COUNT - 1) / PAD_COUNT;
}

static unsigned preset_page_size(unsigned page)
{
    unsigned remaining = synth_factory_count() - preset_page_first(page);
    unsigned capacity = page ? PAD_COUNT : FACTORY_FIRST_PAGE;
    return remaining < capacity ? remaining : capacity;
}

static void preset_reveal(unsigned index)
{
    preset_page = index < FACTORY_FIRST_PAGE ? 0 : 1 + (index - FACTORY_FIRST_PAGE) / PAD_COUNT;
}

static uint32_t plat_ms(void) { return uptime_ms; }
static uint32_t plat_pressed(void) { return button_pressed; }
static uint32_t plat_buttons(void) { return button_state; }
static int plat_enc(unsigned knob) { return fm1_enc_take(knob); }

#define ZIVI_PRO_BOARD
#include "../fm1_base/app.c"

#define OTA_IDENTITY (recovery_mode ? "SMK-37 Pro_000" : FELUCCA_ID)
#include "ota.c"

_Static_assert(OTA_AREA == FL_OTA_LO && OTA_AREA + OTA_AREA_LEN == FL_OTA_HI, "OTA staging map");

static uint32_t ota_now_ms(void) { return plat_ms(); }
static void ota_idle(void) { fm1_wdt_feed(); }

static int ota_erase(uint32_t address)
{
    uint32_t time;
    fm1_wdt_feed();
    return fl_erase4k(address, &time);
}

static int ota_prog(uint32_t address, const void *bytes, uint32_t length)
{
    fm1_wdt_feed();
    return fl_write(address, bytes, length);
}

static int ota_fread(uint32_t address, void *bytes, uint32_t length)
{
    uint32_t state = fm1__lock();
    int rc = FL_FAR(fl_read_ram)(address, bytes, length);
    fm1__unlock(state);
    return rc;
}

static void ota_show(uint32_t step, int32_t code)
{
    static const char *const labels[] = {"", "PACKAGE", "CHECK PACKAGE", "LOADER", "CONFIRM", "RESTART"};
    char message[31];
    if (step >= 9) lcd_format(message, sizeof message, "UPDATE ERROR %d", (int)code);
    else lcd_format(message, sizeof message, "%s", labels[step < 6 ? step : 0]);
    cv_begin(240, 240, IVORY);
    cv_text(15, 85, &FONT_B, "ziviSynthiA SMK", INK);
    cv_text(15, 113, &FONT_S, message, INK);
    lcd_blit(0, 0, 240, 240, pixels[0]);
    rows_sent = LCD_HEIGHT;
}

static void ota_commit(const uint8_t *parameters)
{
    bootguard_clear(&bootguard);
    fm1_irq_off();
    usb_detach();
    fm1_delay_ms(30);
    fm1_enter_update(parameters);
}

static unsigned map_buttons(uint32_t bits)
{
    return ((bits >> BTN_GLOBE) & 1u) << B_GLO |
           ((bits >> BTN_SEQ) & 1u) << B_SEQ |
           ((bits >> BTN_ARP) & 1u) << B_ARP |
           ((bits >> BTN_REC) & 1u) << B_REC |
           ((bits >> BTN_PLAY) & 1u) << B_PLAY |
           ((bits >> BTN_LEFT) & 1u) << B_HOME |
           ((bits >> BTN_RIGHT) & 1u) << B_SAVE |
           ((bits >> BTN_REPEAT) & 1u) << B_LFO |
           ((bits >> BTN_PARA) & 1u) << B_EDIT |
           ((bits >> BTN_FX) & 1u) << B_FX |
           ((bits >> BTN_BANK) & 1u) << B_ENV;
}

static void synth_resume(void)
{
    zivi_muted = zivi_device.muted = false;
    zivi_dirty = 1;
}

static void fill_audio(void)
{
    unsigned slot = zivi_device.write;
    uint32_t start = fm1_ticks();
    if (!zivi_smk_fill(&zivi_device)) return;
    uint32_t duration = (fm1_ticks() - start) / FM1_TICKS_PER_US;
    if (duration > render_max_us) render_max_us = duration;
    const int32_t *words = zivi_device.ring[slot];
    unsigned peak = 0;
    for (unsigned i = 0; i < ZIVI_SMK_WORDS; ++i) {
        int32_t sample = words[i];
        unsigned magnitude = (unsigned)(sample < 0 ? -sample : sample);
        if (magnitude > peak) peak = magnitude;
    }
    if (zivi_module_page && zivi_scope && !zivi_scope_frozen) {
        for (unsigned i = 0; i < ZIVI_SMK_FRAMES; i += TRACE_DECIMATION) {
            zivi_trace[zivi_trace_head] = (int16_t)(words[2 * i] / 512);
            zivi_trace_head = (zivi_trace_head + 1) % TRACE_POINTS;
            if (zivi_trace_count < TRACE_POINTS) ++zivi_trace_count;
        }
    }
    peak >>= 16;
    unsigned monitor = zivi_device.muted ? 0 : (unsigned)(zivi.meter_peak * 66.0);
    if (!zivi_module_page && (zivi_peak != peak || zivi_monitor != monitor)) zivi_dirty = 1;
    zivi_peak = peak;
    zivi_monitor = monitor;
}

void fm1_alnk0_irq(void)
{
    uint8_t pending = fm1_audio_pending();
    fm1_audio_ack_aux(pending);
    if (pending & FM1_AUDIO_HALF) {
        unsigned half = fm1_audio_free_half();
        audio_busy = true;
        RING_PUBLISH();
        int32_t *samples = dma_words + half * ZIVI_SMK_WORDS;
        read_audio(&zivi_device, samples);
        uac_render_start();
        if (uac.feed) {
            int32_t pcm[ZIVI_SMK_WORDS];
            for (unsigned i = 0; i < ZIVI_SMK_WORDS; ++i) pcm[i] = samples[i] >> 8;
            uac_tap(pcm, ZIVI_SMK_FRAMES);
        }
        if (playback_alt && !usb.suspended) pcm_mix(&playback_pcm, samples, playback_gain);
        RING_PUBLISH();
        audio_busy = false;
        fm1_audio_ack_half();
        ++audio_irqs;
    }
}

void fm1_timer5_irq(void)
{
    static unsigned usb_prescale;
    static bool half_ms, usb_due;
    fm1_timer5_ack();
    fm1_input_tick();
    smo_tick();
    if (++usb_prescale == 5) {
        usb_prescale = 0;
        half_ms = !half_ms;
        if (!half_ms) ++uptime_ms;
        usb_due = true;
    }
    if (usb_due && !audio_busy) {
        usb_due = false;
        usb_poll();
    }
}

static void send_midi(unsigned status, unsigned data1, unsigned data2)
{
    unsigned cin = status == 0xf2 ? 3 : status >> 4;
    if (status < 0xf0) status |= midi_channel_out;
    midi_out_event(cin | status << 8 | data1 << 16 | data2 << 24);
}

static void midi_release_panel(void)
{
    for (unsigned k = 0; k < ZIVI_SMK_KEYS; ++k) {
        if (external_keys[k] <= 127) send_midi(0x80, external_keys[k], 0);
        external_keys[k] = 255;
    }
    for (unsigned pad = 0; pad < PAD_COUNT; ++pad) {
        if (daw_pads & (1u << pad)) {
            send_midi(0xa0, 36 + pad, 0);
            send_midi(0x80, 36 + pad, 0);
        }
    }
    daw_pads = 0;
    send_midi(0xb0, 64, 0);
    send_midi(0xb0, 123, 0);
}

static bool midi_patch_cc(unsigned cc, unsigned data)
{
    double value = data / 127.0;
    switch (cc) {
    case 10: synth_module_control(&zivi, M4_OUT1_PAN, 2 * value - 1); break;
    case 71: synth_module_control(&zivi, M4_RESPONSE, value); break;
    case 72: synth_module_control(&zivi, M4_DECAY_MS, 3.0 * exp2(12.0 * value)); break;
    case 73: synth_module_control(&zivi, M4_ATTACK_MS, 2.0 * exp2(9.0 * value)); break;
    case 74: synth_patch_controls(&zivi, zivi.mix, zivi.lfo, 25.0 * exp2(8.6 * value)); break;
    case 91: synth_module_control(&zivi, M4_REVERB_MIX, value); break;
    default: return false;
    }
    return true;
}

static void incoming_program(unsigned channel, unsigned program)
{
    unsigned bank = midi_bank[channel];
    unsigned index = program;
    if (bank) {
        if (bank > preset_pages() || program >= preset_page_size(bank - 1)) return;
        index += preset_page_first(bank - 1);
    }
    if (index >= synth_factory_count()) return;
    zivi_load_preset(4 + (int)index);
    preset_reveal(index);
}

static bool usb_up(void) { return *(volatile uint8_t *)&usb.up != 0; }

static void service_usb_retry(uint32_t now)
{
    static uint32_t restart;
    static bool starting;
    if (usb_up() || usb.detached) return;
    if (starting) {
        if (now - restart >= 25) {
            starting = false;
            usb_start_finish();
        }
    } else if (now - restart >= 1000) {
        restart = now;
        ++usb.retries;
        starting = true;
        usb_start_begin();
    }
}

static bool usb_connected(void)
{
    return usb_up() && *(volatile uint8_t *)&usb.config != 0 && !usb.suspended;
}

static void service_midi(void)
{
    static bool configured;
    bool connected = usb_connected();
    if ((configured && !connected) || midi_in_lost) {
        unsigned state = fm1__lock();
        midi_in_lost = 0;
        mi_r = mi_w;
        fm1__unlock(state);
        zivi_smk_midi_disconnect(&zivi_device);
        memset(midi_bank, 0, sizeof midi_bank);
        midi_clock_input(&midi_clock, &zivi.ks, 0xfc, 0, 0, plat_ms());
        midi_clock.seen = midi_clock.locked = false;
    }
    configured = connected;
    for (unsigned i = 0; i < 16 && mi_r != mi_w; ++i) {
        RING_PUBLISH();
        uint32_t packet = midi_in_q[mi_r % MQ];
        ++mi_r;
        unsigned status = (packet >> 8) & 255, data1 = (packet >> 16) & 127;
        unsigned data2 = (packet >> 24) & 127;
        if (thru_trs) smo_put(packet);
        if (status >= 0xf0u) {
            midi_clock_input(&midi_clock, &zivi.ks, status, data1, data2, plat_ms());
            continue;
        }
        unsigned channel = status & 15u;
        unsigned type = status & 0xf0u;
        if (zivi.midi_channel && zivi.midi_channel != channel + 1) continue;
        if (type == 0xc0u) {
            incoming_program(channel, data1);
        } else if (type == 0xb0u && data1 == 0) {
            midi_bank[channel] = data2;
        } else if (type == 0xb0u && midi_patch_cc(data1, data2)) {
            zivi_preset = -1;
            zivi_dirty = 1;
        } else if (zivi_smk_midi(&zivi_device, status, data1, data2)) {
            if (type == 0x90u && data2) synth_resume();
            zivi_dirty = 1;
        }
    }
}

static void keybed(void)
{
    uint8_t velocity[ZIVI_SMK_KEYS];
    unsigned state = fm1__lock();
    uint64_t keys = fm1_in.keys;
    uint64_t changes = keys ^ key_state;
    if (changes) {
        for (unsigned k = 0; k < ZIVI_SMK_KEYS; ++k) velocity[k] = fm1_in.key_vel[k];
    }
    fm1__unlock(state);
    if (!changes) return;
    key_state = keys;
    for (unsigned k = 0; k < ZIVI_SMK_KEYS; ++k) {
        uint64_t mask = UINT64_C(1) << k;
        if (!(changes & mask)) continue;
        unsigned vel = (keys & mask) ? velocity[k] : 0;
        if (!daw_mode) {
            zivi_smk_key(&zivi_device, k, vel, octave);
            if (vel) synth_resume();
        }
        unsigned note = vel ? (unsigned)(48 + (int)k + 12 * octave) : external_keys[k];
        if (note <= 127) send_midi(vel ? 0x90u : 0x80u, note, vel);
        external_keys[k] = vel ? (uint8_t)note : 255;
    }
}

static void analogue_controls(uint32_t now)
{
    if (now < 300 || fm1_in.frames < 32) return;
    static bool ready;
    if (!ready) {
        for (unsigned i = 0; i < 4; ++i) fader_last[i] = fm1_in.fader[i];
        ready = true;
    }
    for (unsigned i = 0; i < 4; ++i) {
        unsigned value = fm1_in.fader[i];
        if (value == fader_last[i]) continue;
        unsigned prior = fader_last[i];
        if (value != 0 && value != 127 && (value > prior ? value - prior : prior - value) < 3) continue;
        fader_last[i] = (uint8_t)value;
        if (daw_mode) {
            send_midi(0xb0, (daw_fader_bank ? 76 : 70) + i, value);
            continue;
        }
        double ratio = value / 127.0;
        switch (i) {
        case 0: zivi_device.gain_q12 = value * 4096 / 127; break;
        case 1: synth_controls(&zivi, zivi.tune, ratio, zivi.level); break;
        case 2: synth_patch_controls(&zivi, zivi.mix, zivi.lfo, 55.0 * exp2(8.0 * ratio)); break;
        case 3: synth_module_control(&zivi, M4_REVERB_MIX, ratio); break;
        }
        if (i != 0) zivi_preset = -1;
        zivi_dirty = 1;
    }
    int bend = 8192 + smk_pw_bend(&pitch_wheel, &wcal, fm1_in.wheel[0], now, 2, 2);
    if (bend > 16383) bend = 16383;
    if (bend != last_pitch) {
        last_pitch = bend;
        if (!daw_mode) synth_midi(&zivi, 0xe0u | (zivi.midi_channel ? zivi.midi_channel - 1u : 0u), (unsigned)bend & 127, (unsigned)bend >> 7);
        send_midi(0xe0, (unsigned)bend & 127, (unsigned)bend >> 7);
    }
    int mod = smk_mw_val(&mod_wheel, &wcal, fm1_in.wheel[1]);
    if (mod - last_mod >= 2 || last_mod - mod >= 2 || (mod != last_mod && (mod == 0 || mod == 127))) {
        last_mod = mod;
        send_midi(0xb0, 1, (unsigned)mod);
        if (!daw_mode) {
            synth_controls(&zivi, zivi.tune, mod / 127.0, zivi.level);
            zivi_preset = -1;
            zivi_dirty = 1;
        }
    }
}

static void pedal_input(uint32_t now)
{
    if (now < 500) return;
    bool down = smk_pedal_poll(&pedal, fm1_in.pedal, SMK_PEDAL_AUTO, now) != 0;
    if (down == pedal_down) return;
    pedal_down = down;
    send_midi(0xb0, 64, down ? 127 : 0);
    if (down && !daw_mode) {
        synth_resume();
        synth_manual_attack(&zivi);
    }
}

static void hit_pads(void)
{
    uint32_t hits = smk_pick_take();
    for (unsigned pad = 0; hits && pad < 16; ++pad) {
        uint32_t bit = 1u << pad;
        if (!(hits & bit)) continue;
        hits &= ~bit;
        if (zivi_module_page) {
            if (pad < preset_page_size(preset_page))
                zivi_load_preset(4 + (int)(preset_page_first(preset_page) + pad));
        } else if (!zivi_ks_page) {
            if (zivi_smk_pad(&zivi_device, zivi_row, zivi_col, pad, zivi_pin_type)) {
                zivi_row = zivi_row / MATRIX_TILE * MATRIX_TILE + pad / MATRIX_TILE;
                zivi_col = zivi_col / MATRIX_TILE * MATRIX_TILE + pad % MATRIX_TILE;
                zivi_preset = -1;
            }
            zivi_zoom = true;
            zivi_dirty = 1;
        }
    }
}

static void panel_leds(void)
{
    for (unsigned pad = 0; pad < 16; ++pad) {
        if (daw_mode) {
            padled_set(pad, 0, (daw_pads & (1u << pad)) ? 36 : 6, 15);
        } else if (zivi_module_page) {
            unsigned index = preset_page_first(preset_page) + pad;
            bool active = zivi_preset == 4 + (int)index;
            bool available = pad < preset_page_size(preset_page);
            padled_set(pad, active ? 32 : 0, available ? (active ? 32 : 10) : 0,
                       available ? (index >= 6 ? 28 : 10) : 0);
        } else if (zivi_ks_page) {
            padled_set(pad, 0, 0, 0);
        } else {
            unsigned row = zivi_row / MATRIX_TILE * MATRIX_TILE + pad / MATRIX_TILE;
            unsigned col = zivi_col / MATRIX_TILE * MATRIX_TILE + pad % MATRIX_TILE;
            enum Pin pin = zivi.matrix[row][col];
            bool selected = row == zivi_row && col == zivi_col;
            padled_set(pad, pin == PIN_HALF ? 22 : 0, pin == PIN_FULL ? 28 : 0, selected ? 28 : 3);
        }
    }
    fm1_led_key(BTN_REC, zivi_ks_page && zivi.ks.mode == KS_RECORDING);
    fm1_led_key(BTN_PLAY, zivi_ks_page && zivi.ks.mode == KS_PLAYING);
    fm1_led_key(BTN_PATCH, zivi_module_page);
    fm1_led_key(BTN_SEQ, zivi_ks_page);
    fm1_led_key(FM1_BTN_OCT_DOWN, octave < 0);
    fm1_led_key(FM1_BTN_OCT_UP, octave > 0);
}

static void daw_pad_events(uint32_t now)
{
    static uint32_t pressure_ms;
    int event;
    while ((event = smk_extkey_take()) >= 0) {
        unsigned code = (unsigned)event >> 8;
        if (!daw_mode || code < SMK_EXT_DAW || code >= SMK_EXT_DAW + PAD_COUNT) continue;
        unsigned pad = code - SMK_EXT_DAW;
        unsigned velocity = (unsigned)event & 127u, bit = 1u << pad;
        if (velocity) {
            send_midi(0x90, 36 + pad, velocity);
            daw_pads |= bit;
            daw_pressure[pad] = 0;
        } else if (daw_pads & bit) {
            send_midi(0xa0, 36 + pad, 0);
            send_midi(0x80, 36 + pad, 0);
            daw_pads &= ~bit;
        }
    }
    if (!daw_mode || now - pressure_ms < 32) return;
    pressure_ms = now;
    for (unsigned pad = 0; pad < PAD_COUNT; ++pad) {
        if (!(daw_pads & (1u << pad))) continue;
        unsigned pressure = fm1_in.pad_press[pad];
        if (fm1_in.pad_st[pad] == 0) {
            /* A full event queue can drop a release. Reconcile with the
             * scanner after draining the queued edges. */
            send_midi(0xa0, 36 + pad, 0);
            send_midi(0x80, 36 + pad, 0);
            daw_pads &= ~(1u << pad);
        } else if ((pressure > daw_pressure[pad] ? pressure - daw_pressure[pad] :
                    daw_pressure[pad] - pressure) >= 5) {
            daw_pressure[pad] = (uint8_t)pressure;
            send_midi(0xa0, 36 + pad, pressure);
        }
    }
}

static unsigned limit_setting(int value, unsigned maximum)
{
    return value < 0 ? 0 : (unsigned)value > maximum ? maximum : (unsigned)value;
}

static void daw_inputs(uint32_t pressed, uint32_t released)
{
    if (pressed & (1u << BTN_BANK)) daw_knob_bank = !daw_knob_bank;
    if (released & (1u << BTN_FADER_BANK) && !update_armed) daw_fader_bank = !daw_fader_bank;
    unsigned base = daw_knob_bank ? 8 : 0;
    for (unsigned i = 0; i < 8; ++i) {
        int step = plat_enc(i);
        if (step) {
            unsigned value = limit_setting(daw_knobs[base + i] + step, 127);
            daw_knobs[base + i] = (uint8_t)value;
            send_midi(0xb0, 16 + base + i, value);
        }
    }
    if (pressed & (1u << BTN_PLAY)) {
        daw_playing = !daw_playing;
        send_midi(daw_playing ? 0xfa : 0xfc, 0, 0);
    }
    if (pressed & (1u << BTN_REC)) send_midi(0xb0, 115, 127);
    if (pressed & (1u << BTN_LEFT)) send_midi(0xb0, 116, 127);
    if (pressed & (1u << BTN_RIGHT)) send_midi(0xb0, 117, 127);
    if (pressed & (1u << BTN_SEQ)) send_midi(0xb0, 118, 127);
}

static void transport_output(void)
{
    static bool started, note_on;
    static unsigned old_note, old_velocity, old_channel;
    static uint32_t next;
    bool playing = zivi.ks.mode == KS_PLAYING;
    bool clock_running = clock_out && playing && !midi_clock.external && !daw_mode;
    uint32_t ticks = fm1_ticks();
    if (clock_running != started) {
        send_midi(clock_running ? 0xfa : 0xfc, 0, 0);
        next = ticks;
        started = clock_running;
    }
    if (clock_running && (int32_t)(ticks - next) >= 0) {
        send_midi(0xf8, 0, 0);
        uint32_t period = 12500000u / (6u * zivi.ks.speed) * FM1_TICKS_PER_US;
        next += period;
        if ((int32_t)(ticks - next) >= 0) next = ticks + period;
    }
    unsigned velocity = ks_notes_out && playing && !daw_mode ? zivi.ks.velocity : 0;
    unsigned note = limit_setting((int)zivi.ks.note + zivi.ks.transpose, 127);
    if (note_on && old_channel != midi_channel_out) note_on = false;
    if (note_on && (!velocity || note != old_note || velocity != old_velocity)) {
        send_midi(0x80, old_note, 0);
        note_on = false;
    }
    if (velocity && !note_on) {
        old_note = note;
        old_velocity = velocity;
        old_channel = midi_channel_out;
        send_midi(0x90, note, velocity);
        note_on = true;
    }
}

static void midi_settings(void)
{
    int out = plat_enc(EN_K1), in = plat_enc(EN_K2), thru = plat_enc(EN_K3);
    int volume = plat_enc(EN_K4), sync = plat_enc(EN_PRESET), notes = plat_enc(EN_SELECT);
    int clock = plat_enc(EN_ALGO), master = plat_enc(EN_MASTER);
    if (out) {
        unsigned channel = limit_setting((int)midi_channel_out + out, 15);
        if (channel != midi_channel_out) {
            midi_release_panel();
            midi_channel_out = channel;
        }
    }
    if (in) {
        zivi_smk_midi_disconnect(&zivi_device);
        zivi.midi_channel = (uint8_t)limit_setting(zivi.midi_channel + in, 16);
    }
    if (thru) thru_trs = thru > 0;
    if (volume) playback_gain = limit_setting((int)playback_gain + volume * 4, 128);
    if (sync) midi_clock = (SmkMidiClock){.external = sync > 0};
    if (notes) ks_notes_out = notes > 0;
    if (clock) clock_out = clock > 0;
    if (master) zivi_device.gain_q12 = limit_setting((int)zivi_device.gain_q12 + master * 128, 4096);
}

static void panic(void)
{
    midi_release_panel();
    app_panic();
    zivi_device.muted = true;
    unsigned state = fm1__lock();
    zivi_device.read = zivi_device.write;
    fm1__unlock(state);
}

static void panel_input(uint32_t now)
{
    static uint32_t chord_start;
    static bool chord_held;
    uint32_t held = fm1_in.buttons;
    uint32_t released;
    uint32_t pressed = fm1_input_edges(&released);
    bool chord = (held & FM1_OCT_BOTH) == FM1_OCT_BOTH;
    if (chord && !chord_held) chord_start = now;
    if (chord && now - chord_start >= 1800) {
        fm1_audio_stop();
        fm1_irq_off();
        usb_detach();
        bootguard_clear(&bootguard);
        fm1_enter_uboot();
    }
    chord_held = chord;
    if (!chord && (pressed & FM1_OCT_BOTH)) {
        if ((pressed & (1u << FM1_BTN_OCT_DOWN)) && octave > -4) --octave;
        if ((pressed & (1u << FM1_BTN_OCT_UP)) && octave < 3) ++octave;
        zivi_dirty = 1;
    }
    static uint32_t update_start;
    bool arm = (held & (1u << BTN_FADER_BANK)) != 0;
    if (arm && !update_hold) update_start = now;
    if (arm && now - update_start >= 1800u) {
        update_until = now + 60000u;
        update_armed = diagnostics = true;
        update_error = 0;
    }
    update_hold = arm;
    if (update_armed && (int32_t)(now - update_until) >= 0) update_armed = false;
    if (pressed & (1u << BTN_DIAGNOSTICS)) {
        diagnostics = !diagnostics;
        zivi_dirty = 1;
    }
    const unsigned combo = (1u << BTN_GLOBE) | (1u << BTN_PATCH);
    if ((held & combo) == combo && (pressed & combo)) {
        midi_release_panel();
        for (unsigned k = 0; k < ZIVI_SMK_KEYS; ++k)
            zivi_smk_key(&zivi_device, k, 0, octave);
        if (daw_playing) send_midi(0xfc, 0, 0);
        daw_playing = false;
        daw_mode = !daw_mode;
        if (daw_mode) ks_stop(&zivi.ks);
        unsigned state = fm1__lock();
        smk_pad_daw = daw_mode;
        smk_pick_hits = 0;
        fm1__unlock(state);
        diagnostics = false;
        zivi_dirty = 1;
        pressed &= ~combo;
    }
    if (pressed & (1u << BTN_PANIC)) panic();
    if (diagnostics) {
        if (!update_armed && !recovery_mode) midi_settings();
        return;
    }
    if (daw_mode) {
        daw_inputs(pressed, released);
        return;
    }
    if (pressed & (1u << BTN_PATCH)) {
        zivi_module_page = true;
        zivi_ks_page = zivi_scope = false;
        zivi_dirty = 1;
    }
    bool browsing = zivi_module_page && !zivi_scope;
    if (browsing) {
        if (pressed & (1u << BTN_LEFT)) preset_page = (preset_page + preset_pages() - 1) % preset_pages();
        if (pressed & (1u << BTN_RIGHT)) preset_page = (preset_page + 1) % preset_pages();
        if (pressed & ((1u << BTN_LEFT) | (1u << BTN_RIGHT))) zivi_dirty = 1;
        pressed &= ~((1u << BTN_LEFT) | (1u << BTN_RIGHT));
    }
    int previous = zivi_preset;
    button_state = map_buttons(held);
    button_pressed = map_buttons(pressed);
    app_input_only();
    if (browsing && zivi_preset >= 4 && zivi_preset != previous)
        preset_reveal((unsigned)zivi_preset - 4);
    hit_pads();
    int master = plat_enc(EN_MASTER);
    if (master) {
        int gain = (int)zivi_device.gain_q12 + master * 128;
        zivi_device.gain_q12 = gain < 0 ? 0 : gain > 4096 ? 4096 : (unsigned)gain;
        zivi_dirty = 1;
    }
}

static void display_diagnostics(void)
{
    char line[48];
    cv_begin(240, 240, IVORY);
    cv_text(12, 10, &FONT_B, "MIDI / USB AUDIO", INK);
    snprintf(line, sizeof line, "USB %s   REC %s   PLAY %s", usb_connected() ? "ON" : "OFF",
             uac.alt ? (uac.r48 ? "48K" : "44K") : "-", playback_alt ? "44K" : "-");
    cv_text(12, 35, &FONT_XS, line, INK);
    snprintf(line, sizeof line, "DSP %u / 2902 us  UNDERRUNS %u", (unsigned)render_max_us, zivi_device.underruns);
    cv_text(12, 53, &FONT_XS, line, INK);
    snprintf(line, sizeof line, "AUDIO IRQ %u   USB XRUN %u", (unsigned)audio_irqs,
             (unsigned)(uac.underruns + uac.overruns + playback_pcm.underruns + playback_pcm.overruns));
    cv_text(12, 71, &FONT_XS, line, INK);
    if (zivi.midi_channel) snprintf(line, sizeof line, "K1 TX %02u   K2 RX %02u", midi_channel_out + 1, (unsigned)zivi.midi_channel);
    else snprintf(line, sizeof line, "K1 TX %02u   K2 RX ALL", midi_channel_out + 1);
    cv_text(12, 99, &FONT_XS, line, INK);
    snprintf(line, sizeof line, "K3 THRU %s   K4 USB %u%%", thru_trs ? "ON" : "OFF", playback_gain * 100 / 128);
    cv_text(12, 118, &FONT_XS, line, INK);
    snprintf(line, sizeof line, "K5 SYNC %s   K6 KS NOTE %s",
             midi_clock.external ? (midi_clock.locked ? "USB*" : "USB") : "INT", ks_notes_out ? "ON" : "OFF");
    cv_text(12, 137, &FONT_XS, line, INK);
    snprintf(line, sizeof line, "K7 CLOCK %s   K8 SYNTH %u%%", clock_out ? "ON" : "OFF", zivi_device.gain_q12 * 100 / 4096);
    cv_text(12, 156, &FONT_XS, line, INK);
    if (update_armed || recovery_mode)
        cv_text(12, 182, &FONT_S, "USB UPDATE READY", BLUE);
    else if (update_error) {
        snprintf(line, sizeof line, "UPDATE ERROR %d", update_error);
        cv_text(12, 182, &FONT_S, line, RED);
    }
    cv_text(12, 213, &FONT_XS, "SCALE EXIT   OCT-/+ UBOOT", INK);
    cv_blit(0, 0);
}

static void display_daw(void)
{
    char line[40];
    cv_begin(240, 240, IVORY);
    cv_text(14, 16, &FONT_B, "MIDI CONTROLLER", INK);
    snprintf(line, sizeof line, "CH %02u    USB %s    OCT %+d", midi_channel_out + 1, usb_connected() ? "ON" : "OFF", octave);
    cv_text(14, 45, &FONT_S, line, INK);
    snprintf(line, sizeof line, "KNOBS %s    FADERS %s", daw_knob_bank ? "B" : "A", daw_fader_bank ? "B" : "A");
    cv_text(14, 69, &FONT_S, line, INK);
    cv_text(14, 101, &FONT_XS, "PADS 36-51 + PRESSURE", INK);
    cv_text(14, 124, &FONT_XS, "PLAY TRANSPORT   REC CC115", INK);
    cv_text(14, 213, &FONT_XS, "GLOBE + PATCH EXIT", INK);
    cv_blit(0, 0);
}

static void fm1_fault(const fm1_crash_t *crash)
{
    (void)crash;
    fm1_audio_stop();
    fm1_reboot();
}

extern uint32_t _data_start[], _data_end[], _data_load[], _bss_start[], _bss_end[];
extern uint32_t _pool_start[], _pool_end[], _rt_start[], _rt_end[], _rt_load[];
extern void isr_alnk0(void), isr_timer5(void);

void fm1_cstart(void)
{
    fm1_cpu1_halt();
    fm1_time_init();
    fm1_supplies_stock();
    fm1_reset_reason();
    fm1_wdt_arm(0x0d);
    if ((fm1_boot.p3_rst & 1) && !(fm1_boot.p3_rst & (4 | 0x40))) bootguard_clear(&bootguard);
    uint32_t boot_mode = bootguard_begin(&bootguard);
    if (boot_mode == BOOT_ROM) fm1_enter_uboot();
    fm1_irq_init();
    for (uint32_t *p = _bss_start; p < _bss_end; ++p) *p = 0;
    for (uint32_t *p = _pool_start; p < _pool_end; ++p) *p = 0;
    const uint32_t *src = _data_load;
    for (uint32_t *p = _data_start; p < _data_end; ++p) *p = *src++;
    src = _rt_load;
    for (uint32_t *p = _rt_start; p < _rt_end; ++p) *p = *src++;
    fm1_mailbox_clear();
    fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS | FM1_GUARD_PC);

    lcd_init();
    fm1_input_init();
    uint8_t trim[0x22c];
    if (FL_FAR(fl_read_ram)(SMK_TRIM_FLASH, trim, sizeof trim) == 0) smk_trim_load(trim);
    smk_pad_pick = 0xffff;
    app_init();
    memset(external_keys, 255, sizeof external_keys);
    memset(daw_knobs, 64, sizeof daw_knobs);
    recovery_mode = boot_mode == BOOT_RECOVERY;
    diagnostics = recovery_mode;
    usb_start();
    smk_uart1_tx_init();
    lcd_ready_tick = fm1_ticks();
    if (!recovery_mode) {
        fill_audio();
        fill_audio();
        fm1_audio_init(dma_words, ZIVI_SMK_WORDS, isr_alnk0, 3);
    }
    fm1_timer5_start(isr_timer5, 4);
    fm1_guard_lock_top();
    fm1_irq_enable_all();

    uint32_t last_control = 0, last_display = 0, last_led = 0;
    for (;;) {
        fm1_wdt_feed();
        service_midi();
        uint32_t now = plat_ms();
        service_usb_retry(now);
        panel_input(now);
        if (!recovery_mode) {
            keybed();
            daw_pad_events(now);
            midi_clock_timeout(&midi_clock, &zivi.ks, now);
            fill_audio();
            transport_output();
        }
        ota_service();
        if (usb.ota_req) {
            usb.ota_req = 0;
            if (update_armed || recovery_mode) {
                update_armed = false;
                uint32_t state = fm1__lock();
                uint32_t id = FL_FAR(fl_jedec_ram)();
                fm1__unlock(state);
                if (FL_JEDEC_OK(id)) {
                    fm1_audio_stop();
                    zivi_device.read = zivi_device.write;
                    update_error = ota_session();
                    if (!recovery_mode) {
                        memset(dma_words, 0, sizeof dma_words);
                        fill_audio();
                        fill_audio();
                        /* The vector was attached at boot. Unlock its page during reattach. */
                        fm1_irq_off();
                        fm1_guard_unlock_top();
                        fm1_audio_init(dma_words, ZIVI_SMK_WORDS, isr_alnk0, 3);
                        fm1_guard_lock_top();
                        fm1_irq_enable_all();
                    }
                } else update_error = -16;
            }
        }
        if (!recovery_mode && now - last_control >= 8) {
            last_control = now;
            analogue_controls(now);
            pedal_input(now);
        }
        if (now - last_led >= 30 && rows_sent == LCD_HEIGHT) {
            last_led = now;
            panel_leds();
            padled_flush();
        }
        if (rows_sent == LCD_HEIGHT && now - last_display >= 105) {
            last_display = now;
            if (diagnostics) display_diagnostics();
            else if (daw_mode) display_daw();
            else zivi_update_frame();
        }
        /* A stripe takes about 1.3 ms. Give DMA time to finish before issuing
         * the next commands so that the producer can keep two halves ready. */
        if (rows_sent < LCD_HEIGHT && (int32_t)(fm1_ticks() - lcd_ready_tick) >= 0) {
            screen_send_stripe();
            lcd_ready_tick = fm1_ticks() + 1330 * FM1_TICKS_PER_US;
        }
        if (!recovery_mode && now > 3000 && audio_irqs && bootguard.pending) bootguard_clear(&bootguard);
    }
}
