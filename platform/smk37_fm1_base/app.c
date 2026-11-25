#include "app.h"
#include "master.h"
#include "../../app/core/dsp_curve.h"
#include "../smk37_pro/zivi_smk37.h"
#include "../smk37_pro/midi_clock.h"

#include <stdio.h>
#ifndef PLAT_HOST
#define ZIVI_BSP_LIBC
#else
#include <stdatomic.h>
#include <string.h>
#endif

static uint32_t label_bits(float value)
{
    union { float value; uint32_t bits; } encoded = {value};
    return encoded.bits;
}

#define ZIVI_LCD_FLOAT_BITS
#define ZIVI_LABEL(value) label_bits(value)
#include "../smk37_pro/format.h"

#define snprintf lcd_format
#define exp expf
#define exp2 dsp_exp2
#define lround lroundf
#define sin sinf
#define cos cosf
#define sqrt sqrtf
#define log logf
#define fmax fmaxf
#define fmin fminf

static ZiviSmk37 zivi_device;
static unsigned zivi_monitor_bus, zivi_monitor_bus_drawn;
static uint32_t synth_activity_ms;
static uint32_t panel_buttons, panel_pressed;
static uint32_t plat_pressed(void) { return panel_pressed; }
static uint32_t panel_instrument_buttons(void) { return panel_buttons; }

#define ZIVI_PRO_BOARD
#define ZIVI_OPEN_PANEL
#define ZIVI_FELUCCA_PANEL
#define ZIVI_PANEL_PALETTE
#define IVORY RGB(228, 225, 214)
#define INK RGB(32, 35, 36)
#define SILVER RGB(198, 198, 187)
#define BLACK RGB(27, 30, 32)
#define GRAPHITE RGB(85, 90, 90)
#define BLUE RGB(36, 103, 169)
#define RED RGB(171, 45, 45)
#define GREEN RGB(40, 123, 78)
#define AMBER RGB(224, 171, 66)
#define WHITE RGB(246, 243, 229)
#define MATRIX_TILE_COLOR RGB(40, 56, 61)
#define DARK_BLUE RGB(30, 58, 77)
#define PANEL_CYAN RGB(112, 199, 216)
#define PANEL_GRID RGB(66, 78, 78)
#define PANEL_GREEN RGB(75, 188, 128)
static void rack_input(uint32_t pressed);
static void rack_draw(void);
static void rack_knob(int cx, int cy, int radius, uint16_t color, float position);
#define plat_buttons panel_instrument_buttons
#define app_init instrument_init
#define app_panic instrument_panic
#define app_input_only instrument_input
#define app_frame instrument_frame
#define app_ui_init instrument_ui_init
#define app_expect instrument_expect
#include "../fm1_base/app.c"
#undef app_expect
#undef app_frame
#undef app_ui_init
#undef app_input_only
#undef app_panic
#undef app_init
#undef plat_buttons

#include "module_panel.h"

enum { USB_INPUT_SLOTS = 4 };
static int32_t usb_input_ring[USB_INPUT_SLOTS][2 * APP_BLOCK];
static float usb_input_left[APP_BLOCK], usb_input_right[APP_BLOCK];
static volatile unsigned usb_input_read, usb_input_write, usb_input_dropped;
static volatile bool usb_matrix;

static inline void usb_input_fence(void)
{
#ifdef PLAT_HOST
    atomic_signal_fence(memory_order_seq_cst);
#else
    app_usb_platform_fence();
#endif
}

int app_usb_matrix_active(void)
{
    return usb_matrix;
}

/* The codec ISR writes one resampled USB block. The main loop owns the DSP. */
int32_t *app_usb_input_buffer(void)
{
    unsigned write = usb_input_write;
    if ((write + 1) % USB_INPUT_SLOTS == usb_input_read) {
        ++usb_input_dropped;
        return NULL;
    }
    usb_input_fence();
    return usb_input_ring[write];
}

void app_usb_input_ready(void)
{
    usb_input_fence();
    usb_input_write = (usb_input_write + 1) % USB_INPUT_SLOTS;
}

static void usb_matrix_set(bool enabled)
{
    uint32_t lock = plat_lock();
    usb_input_read = usb_input_write = usb_input_dropped = 0;
    usb_matrix = enabled;
    plat_unlock(lock);
    zivi_device.patch_transition = true;
}

enum { STUDIO_STORAGE = 4, STUDIO_BUSY = 5 };
static bool user_slot_present(unsigned slot);
static unsigned user_slot_operation(unsigned slot, bool save, bool grid_only);
#include "controls.h"
#include "studio.h"

static MasterLimiter capture_limiter = {.gain = UNITY_Q12, .limit = UNITY_Q12};

void app_init(void)
{
    instrument_init();
    modules_set_profile(&zivi.modules, zivi.rate, true);
    rack_page = RACK_OSC1;
    rack_k8_delta = 0;
    controls_init();
    usb_matrix_set(false);
    plat_usb_audio_full(1);
    audio_live = false;
}

void app_input_only(void)
{
    poll_midi();
    poll_panel();
    poll_keys();
    uint32_t now = plat_ms();
    service_manual_attack(now);
    service_transport(now);
    for (unsigned i = 0; i < ZIVI_SMK_SLOTS - 1; ++i) {
        unsigned out_read = zivi_device.read, out_write = zivi_device.write;
        if ((out_write + 1) % ZIVI_SMK_SLOTS == out_read) break;
        unsigned input_read = usb_input_read;
        bool input_ready = usb_matrix && input_read != usb_input_write;
        unsigned out_ready = (out_write + ZIVI_SMK_SLOTS - out_read) % ZIVI_SMK_SLOTS;
        if (audio_live && !daw_mode && !zivi_device.muted && out_ready < profile_min_ready)
            profile_min_ready = out_ready;
        /* After panic, prime two silent-input blocks while waiting on UAC.
         * Otherwise pace the matrix from codec-sized blocks, not the UI loop. */
        if (usb_matrix && !input_ready && out_ready >= 2) break;
        int synth_step = (int)synth_gain - (int)zivi_device.gain_q12;
        zivi_device.gain_q12 = (unsigned)((int)zivi_device.gain_q12 + clamp(synth_step, -128, 128));
        int previous_usb_gain = (int)usb_render_gain;
        int usb_step = (int)usb_play_gain - previous_usb_gain;
        if (usb_step) {
            usb_render_gain = (unsigned)((int)usb_render_gain + clamp(usb_step, -128, 128));
            plat_usb_play_gain(usb_render_gain);
        }
        uint32_t start = plat_ticks24();
        if (usb_matrix) {
            if (input_ready) {
                usb_input_fence();
                const int32_t *pcm = usb_input_ring[input_read];
                float gain = previous_usb_gain * (1.0f / (4096.0f * 8388608.0f));
                float step = ((int)usb_render_gain - previous_usb_gain) *
                             (1.0f / (APP_BLOCK * 4096.0f * 8388608.0f));
                for (unsigned frame = 0; frame < APP_BLOCK; ++frame) {
                    usb_input_left[frame] = pcm[2 * frame] * gain;
                    usb_input_right[frame] = pcm[2 * frame + 1] * gain;
                    gain += step;
                }
                usb_input_fence();
                usb_input_read = (input_read + 1) % USB_INPUT_SLOTS;
            } else {
                memset(usb_input_left, 0, sizeof usb_input_left);
                memset(usb_input_right, 0, sizeof usb_input_right);
            }
            if (!zivi_smk_fill_input(&zivi_device, usb_input_left, usb_input_right)) break;
        } else if (!zivi_smk_fill(&zivi_device)) break;
        uint32_t elapsed = plat_ticks24() - start;
        if (elapsed > peak_ticks) peak_ticks = elapsed;
        ++profile_blocks;
        if (elapsed >= (24000000u * (uint64_t)APP_BLOCK) / ZIVI_SMK_RATE) ++profile_over_budget;
        unsigned bus = (unsigned)(zivi.meter_peak * 53.0f);
        if (bus > zivi_monitor_bus) zivi_monitor_bus = bus;
        const int32_t *pcm = zivi_device.ring[(zivi_device.write + ZIVI_SMK_SLOTS - 1) % ZIVI_SMK_SLOTS];
        for (unsigned f = 0; f < ZIVI_SMK_FRAMES; f += TRACE_DECIMATION) {
            int32_t value = pcm[2 * f] >> 9;
            int32_t right = pcm[2 * f + 1];
            unsigned left_peak = (unsigned)__builtin_abs(pcm[2 * f]) >> 17;
            unsigned right_peak = (unsigned)__builtin_abs(right) >> 17;
            if (left_peak > zivi_peak) zivi_peak = left_peak;
            if (right_peak > zivi_monitor) zivi_monitor = right_peak;
            if (left_peak > 2 || right_peak > 2) synth_activity_ms = now;
            if (zivi_scope && !zivi_scope_frozen) {
                zivi_trace[zivi_trace_head] = (int16_t)clamp(value, -16384, 16383);
                zivi_trace_head = (zivi_trace_head + 1u) % TRACE_POINTS;
                if (zivi_trace_count < TRACE_POINTS) ++zivi_trace_count;
            }
        }
    }
    audio_live = true;
}

void app_render(int32_t *lr, uint32_t frames, uint32_t master_q12)
{
    (void)master_q12;
    if (!audio_live || frames != APP_BLOCK) {
        memset(lr, 0, (size_t)frames * 2u * sizeof *lr);
        return;
    }
    (void)PLAT_FAR(zivi_smk_irq_read)(&zivi_device, lr);
    for (unsigned i = 0; i < 2u * APP_BLOCK; ++i) lr[i] *= 2;
    master_limit_block(&capture_limiter, lr);
}

void app_limit_master(int32_t *pcm, uint32_t master_q12)
{
    static int previous_gain;
    master_limit_block(&master_limiter, pcm);
    int ramp = previous_gain * APP_BLOCK;
    int step = (int)master_q12 - previous_gain;
    for (unsigned frame = 0; frame < APP_BLOCK; ++frame) {
        int gain = ramp >> 7;
        pcm[2 * frame] = q12_scale(pcm[2 * frame], gain);
        pcm[2 * frame + 1] = q12_scale(pcm[2 * frame + 1], gain);
        ramp += step;
    }
    previous_gain = (int)master_q12;
}

void app_panic(void)
{
    stop_sync();
    release_outputs();
    instrument_panic();
    synth_seq_play(&zivi, false);
    seq_entry = seq_follow = false;
    synth_manual_hold(&zivi, false);
    zivi.modules.manual_attack = zivi.modules.manual_cycle = false;
    panel_attack = web_attack = false;
    zivi_device.muted = true;
    uint32_t lock = plat_lock();
    zivi_device.read = zivi_device.write;
    usb_input_read = usb_input_write;
    plat_unlock(lock);
}

#include "panel_views.h"
/* The BSP only calls this when the strip has a dirty frame. Refreshes share
 * the LCD bus. Keep at least four audio blocks ready and space the transfers. */
int app_led_flush_ready(void)
{
    static uint32_t last_flush;
    unsigned write = zivi_device.write, read = zivi_device.read;
    unsigned ready = (write + ZIVI_SMK_SLOTS - read) % ZIVI_SMK_SLOTS;
    uint32_t now = plat_ms();
    const Arp *arp = &zivi.arp;
    bool sounding = zivi.held_count || zivi.sequencer.playing || zivi.ks.mode != KS_STOPPED ||
                   (arp->enabled && (arp->free_run || arp->latched_count)) ||
                   now - synth_activity_ms < 200 || panel_attack || web_attack;
    if (ready < 4 || now - last_flush < 96 || (pad_led_mode == PAD_LED_QUIET && sounding)) return 0;
    last_flush = now;
    return 1;
}


void app_ui_init(void)
{
    if (!splash_active) instrument_ui_init();
}

void app_frame(void)
{
    static uint32_t last_update, last_status;
    static unsigned last_arp_note = NO_NOTE, last_step = NO_NOTE;
    static bool last_rec;
    app_input_only();
    uint32_t now = plat_ms();
    if (splash_animate(now)) return;
    if (now - last_update < 40) return;
    update_pad_lights();
    last_update = now;
    if ((view == VIEW_ARP || view == VIEW_SEQ) && last_rec != zivi.sequencer.recording)
        zivi_dirty = 1;
    last_rec = zivi.sequencer.recording;
    if (view == VIEW_ARP) {
        unsigned note = zivi.arp.velocity ? zivi.arp.note : NO_NOTE;
        if (note != last_arp_note) zivi_dirty = 1;
        last_arp_note = note;
    }
    if (view == VIEW_SEQ || (view == VIEW_ARP && last_rec)) {
        unsigned step = zivi.sequencer.playing ? zivi.sequencer.index : NO_NOTE;
        if (step != last_step) zivi_dirty = 1;
        last_step = step;
    }
    if (view == VIEW_PLAY && !daw_mode) {
        if (zivi_module_page && !zivi_scope && rack_shows_master()) {
            unsigned master = rack_master_percent();
            if (master != rack_master_drawn) zivi_dirty = 1;
        }
        if (!zivi_module_page && !zivi_ks_page && zivi_monitor_bus != zivi_monitor_bus_drawn)
            zivi_dirty = 1;
        zivi_update_frame();
        if (!zivi_module_page && !zivi_ks_page && (zivi_peak || zivi_monitor))
            zivi_dirty = 1;
        zivi_peak = zivi_peak > 4 ? zivi_peak - 4 : 0;
        zivi_monitor = zivi_monitor > 4 ? zivi_monitor - 4 : 0;
        zivi_monitor_bus = zivi_monitor_bus > 4 ? zivi_monitor_bus - 4 : 0;
    } else if (zivi_dirty || ((view == VIEW_SCALE || view == VIEW_RADIO ||
               (midi_clock.external && (view == VIEW_ARP || view == VIEW_SEQ || daw_mode))) && now - last_status >= 250)) {
        draw_overlays();
        zivi_dirty = 0;
        last_status = now;
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (strcmp(what, "zmiss") == 0) *got = (int)zivi_device.underruns;
    else if (strcmp(what, "zusbmatrix") == 0) *got = usb_matrix;
    else if (strcmp(what, "zusbdrop") == 0) *got = (int)usb_input_dropped;
    else if (strcmp(what, "zview") == 0) *got = (int)view;
    else if (strcmp(what, "zbank") == 0) *got = (int)page + 1;
    else if (strcmp(what, "zcursor") == 0) *got = (int)cursor + 1;
    else if (strcmp(what, "zdaw") == 0) *got = daw_mode;
    else if (strcmp(what, "zdawscene") == 0) *got = daw_scene;
    else if (strcmp(what, "zseqclockbpm") == 0) *got = zivi.sequencer.clock_bpm;
    else if (strcmp(what, "zarpclockbpm") == 0) *got = zivi.clock_bpm;
    else if (strcmp(what, "zclock") == 0) *got = midi_clock.locked;
    else if (strcmp(what, "zclockref") == 0) *got = ks_clock_reference;
    else if (strcmp(what, "zsyncbpm") == 0) *got = (int)sync_bpm();
    else if (strcmp(what, "zsyncrun") == 0) *got = sync_running;
    else if (strcmp(what, "zsyncpos") == 0) *got = (int)sync_position;
    else if (strcmp(what, "zsplash") == 0) *got = splash_active;
    else if (strcmp(what, "zsplashframe") == 0) *got = (int)splash_frame;
    else if (strcmp(what, "zroute") == 0) *got = (int)port_mode;
    else if (strcmp(what, "zsynthgain") == 0) *got = (int)zivi_device.gain_q12;
    else if (strcmp(what, "zusbgain") == 0) *got = (int)usb_render_gain;
    else if (strcmp(what, "zmaster") == 0) *got = (int)plat_master();
    else if (strcmp(what, "zarp") == 0) *got = zivi.arp.enabled;
    else if (strcmp(what, "zarpbpm") == 0) *got = (int)zivi.arp.bpm;
    else if (strcmp(what, "zchance") == 0) *got = zivi.arp.chance;
    else if (strcmp(what, "zhold") == 0) *got = zivi.arp.hold;
    else if (strcmp(what, "zfree") == 0) *got = zivi.arp.free_run;
    else if (strcmp(what, "zarpkey") == 0) *got = zivi.arp.velocity ? (int)zivi.arp.note : -1;
    else if (strcmp(what, "zseqrecording") == 0) *got = zivi.sequencer.recording;
    else if (strcmp(what, "zledmode") == 0) *got = (int)pad_led_mode;
    else if (strcmp(what, "zarporder") == 0) *got = zivi.arp.order;
    else if (strcmp(what, "zarpdivision") == 0) *got = zivi.arp.division;
    else if (strcmp(what, "zseqlength") == 0) *got = zivi.sequencer.length;
    else if (strcmp(what, "zseqcursor") == 0) *got = seq_cursor + 1;
    else if (strcmp(what, "zseqindex") == 0) *got = zivi.sequencer.index;
    else if (strcmp(what, "zseqplaying") == 0) *got = zivi.sequencer.playing;
    else if (strcmp(what, "zseqkey") == 0) *got = zivi.sequencer.steps[seq_cursor].note;
    else if (strcmp(what, "zseqvel") == 0) *got = zivi.sequencer.steps[seq_cursor].velocity;
    else if (strcmp(what, "zksrandom") == 0) *got = zivi.ks.random_held;
    else if (strcmp(what, "zksview") == 0) *got = zivi_ks_page;
    else if (strcmp(what, "zscale") == 0) *got = scale_mode;
    else if (strcmp(what, "zscaleroot") == 0) *got = (int)scale_root;
    else if (strcmp(what, "zattackheld") == 0) *got = zivi.modules.manual_hold;
    else if (strcmp(what, "zstick") == 0) *got = (int)(zivi.stick_x * 1000);
    else if (strcmp(what, "zsticky") == 0) *got = (int)(zivi.stick_y * 1000);
    else if (strcmp(what, "zksmatrix") == 0) *got = zivi.ks_matrix;
    else if (strcmp(what, "zksosc") == 0) *got = zivi.ks_routes[DST_OSC1_PITCH];
    else if (strcmp(what, "zcut") == 0) *got = (int)zivi.cutoff;
    else if (strcmp(what, "zrack") == 0) *got = rack_page + 1;
    else if (strcmp(what, "zscopefrozen") == 0) *got = zivi_scope_frozen;
    else if (strncmp(what, "zrackk", 6) == 0 && what[6] >= '1' && what[6] <= '8' && !what[7]) {
        const RackDial *dial = &rack_controls[rack_page][what[6] - '1'];
        *got = dial->label ? (int)lround(rack_value(dial->param) * 100.0f) : -1;
    }
    else if (strcmp(what, "zosc1hz") == 0) *got = (int)zivi.modules.p[M4_OSC1_HZ];
    else if (strcmp(what, "ztrigger") == 0) *got = (int)zivi.modules.p[M4_TRIGGER_MODE];
    else if (strcmp(what, "zattacktime") == 0) *got = (int)zivi.modules.p[M4_ATTACK_MS];
    else if (strcmp(what, "zrev") == 0) *got = (int)(zivi.modules.p[M4_REVERB_MIX] * 127.0f + 0.5f);
    else if (strncmp(what, "zled", 4) == 0) {
        unsigned p = 0;
        for (const char *s = what + 4; *s >= '0' && *s <= '9'; ++s) p = p * 10 + (unsigned)(*s - '0');
        if (p >= NPADS) return 0;
        *got = (int)last_pad_light[p];
    }
    else return instrument_expect(what, got);
    return 1;
}
#endif

int app_master_turn(int delta)
{
    if (view != VIEW_PLAY) return 0;
    if (!daw_mode && zivi_module_page && !zivi_scope && !rack_shows_master() &&
        !(plat_buttons() & (1u << B_EDIT))) {
        rack_k8_delta = clamp(rack_k8_delta + delta, -127, 127);
        return 1;
    }
    if (!daw_mode || !(buttons_before & ((1u << B_KBANK) | (1u << B_EDIT)))) return 0;
    daw_master_delta = clamp(daw_master_delta + delta, -127, 127);
    return 1;
}
