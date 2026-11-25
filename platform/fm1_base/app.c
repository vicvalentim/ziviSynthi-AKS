#include "../../app/core/presets.h"

#include <math.h>
#include <stdio.h>
#ifndef ZIVI_BSP_LIBC
#include <string.h>
#endif

#ifndef ZIVI_LABEL
#define ZIVI_LABEL(value) ((double)(value))
#endif

#ifndef ZIVI_PANEL_PALETTE
#define IVORY RGB(233, 231, 221)
#define INK RGB(29, 35, 38)
#define SILVER RGB(199, 201, 197)
#define BLACK RGB(17, 19, 21)
#define GRAPHITE RGB(68, 77, 81)
#define BLUE RGB(40, 121, 184)
#define RED RGB(190, 52, 64)
#define GREEN RGB(57, 150, 91)
#define AMBER RGB(242, 193, 91)
#define WHITE RGB(246, 244, 235)
#define MATRIX_TILE_COLOR RGB(22, 39, 50)
#endif

#ifdef ZIVI_PRO_BOARD
#define zivi (zivi_device.synth)
#else
static Synth zivi;
#endif
static uint32_t zivi_keys;
static uint32_t zivi_buttons;
static unsigned zivi_row, zivi_col;
static bool zivi_module_page, zivi_scope, zivi_scope_frozen;
enum { TRACE_POINTS = 128, TRACE_DECIMATION = 4, TRACE_PERIOD_MS = 40 };
static int16_t zivi_trace[TRACE_POINTS];
static unsigned zivi_trace_head, zivi_trace_count, zivi_trace_skip;
static uint32_t zivi_last_visual;
static unsigned zivi_hz_display[4];
static int zivi_preset;
static const char *const default_presets[] = {"INITIAL", "SPRING", "TRAPEZOID LOOP", "KS LIVE"};
static unsigned zivi_param;
static enum EnvelopePhase zivi_phase;
static bool zivi_zoom, zivi_advanced, zivi_muted, zivi_ks_page;
#ifdef ZIVI_FELUCCA_PANEL
static unsigned zivi_ks_dest = DST_OSC1_PITCH;
#endif
static unsigned zivi_ks_progress, zivi_ks_count;
static enum KsMode zivi_ks_mode;
static uint32_t zivi_pad_keys;
static enum Pin zivi_pin_type;
static unsigned zivi_peak, zivi_monitor;
static int zivi_dirty;

void app_init(void)
{
#ifdef ZIVI_PRO_BOARD
    zivi_smk_init(&zivi_device);
#else
    synth_init(&zivi, 44100.0);
#endif
    zivi_keys = zivi_buttons = zivi_pad_keys = 0;
    zivi_row = SRC_ENVELOPE;
    zivi_col = DST_OUT1_IN;
    zivi_peak = zivi_monitor = 0;
    zivi_module_page = zivi_scope = zivi_scope_frozen = false;
    zivi_trace_head = zivi_trace_count = zivi_trace_skip = 0;
    zivi_last_visual = plat_ms();
    memset(zivi_hz_display, 0, sizeof zivi_hz_display);
    zivi_preset = 0;
    zivi_param = 0;
    zivi_phase = ENV_OFF;
    zivi_zoom = zivi_advanced = zivi_muted = zivi_ks_page = false;
    zivi_ks_progress = zivi_ks_count = 0;
    zivi_ks_mode = KS_STOPPED;
    zivi_pin_type = PIN_FULL;
    zivi_dirty = 1;
}

#ifndef ZIVI_PRO_BOARD
void app_render(int32_t *lr, uint32_t frames, uint32_t master_q12)
{
    uint32_t packet;
    for (unsigned i = 0; i < 64 && plat_midi_in(&packet, NULL); ++i) {
        if (synth_midi(&zivi, (packet >> 8) & 0xff, (packet >> 16) & 0xff, packet >> 24)) {
            zivi_dirty = 1;
        }
    }

    if (zivi_muted) {
        memset(lr, 0, (size_t)frames * 2 * sizeof *lr);
        return;
    }
    float left[APP_BLOCK], right[APP_BLOCK];
    double monitor = 0;
    unsigned peak = 0;
    float master_gain = (float)master_q12 * (8388607.0f / 4096.0f);
    for (uint32_t offset = 0; offset < frames;) {
        uint32_t size = frames - offset > APP_BLOCK ? APP_BLOCK : frames - offset;
        synth_render_stereo(&zivi, left, right, size);
        monitor = fmax(monitor, zivi.meter_peak);
        for (uint32_t i = 0; i < size; ++i) {
            int32_t a = (int32_t)(left[i] * master_gain);
            int32_t c = (int32_t)(right[i] * master_gain);
            lr[2 * (offset + i)] = a;
            lr[2 * (offset + i) + 1] = c;
            if (zivi_module_page && zivi_scope && !zivi_scope_frozen) {
                if (zivi_trace_skip == 0) {
                    zivi_trace[zivi_trace_head] = (int16_t)(left[i] * 16384.0f);
                    zivi_trace_head = (zivi_trace_head + 1) % TRACE_POINTS;
                    if (zivi_trace_count < TRACE_POINTS) ++zivi_trace_count;
                }
                zivi_trace_skip = (zivi_trace_skip + 1) % TRACE_DECIMATION;
            }
            unsigned magnitude = (unsigned)fmax(fabs((double)a), fabs((double)c));
            if (magnitude > peak) peak = magnitude;
        }
        offset += size;
    }
    peak >>= 16;
    unsigned monitor_display = (unsigned)(monitor * 66.0);
    if (!zivi_module_page && (zivi_peak != peak || zivi_monitor != monitor_display)) zivi_dirty = 1;
    zivi_peak = peak;
    zivi_monitor = monitor_display;
}

#endif

void app_panic(void)
{
#ifdef ZIVI_PRO_BOARD
    zivi_smk_panic(&zivi_device);
#else
    synth_all_notes_off(&zivi);
    ks_stop(&zivi.ks);
    zivi.modules.env = 0;
    zivi.modules.env_running = false;
    zivi.modules.phase = ENV_OFF;
#endif
    zivi_muted = true;
    zivi_peak = zivi_monitor = 0;
    zivi_trace_count = 0;
    zivi_dirty = 1;
}

static void zivi_load_preset(int index)
{
    zivi_preset = index;
#ifdef ZIVI_OPEN_PANEL
    zivi_device.patch_transition = true;
#endif
#ifdef ZIVI_PRO_BOARD
    zivi_device.muted = zivi_muted = false;
#endif
    switch (index) {
    case 0: synth_safe_patch(&zivi); break;
    case 1: synth_spring_patch(&zivi); break;
    case 2: synth_repeat_patch(&zivi); break;
    case 3: synth_ks_patch(&zivi); break;
    default: synth_factory_apply(&zivi, (unsigned)index - 4); break;
    }
    zivi_dirty = 1;
}

void app_input_only(void)
{
    uint32_t buttons = plat_buttons();
#ifdef ZIVI_PRO_BOARD
    uint32_t pressed = plat_pressed();
#else
    uint32_t keys = plat_keys();
    uint32_t key_changes = keys ^ zivi_keys;
    uint32_t pressed = buttons & ~zivi_buttons;
    zivi_keys = keys;
#endif
    zivi_buttons = buttons;

    if (pressed & (1u << B_GLO)) {
        zivi_module_page = !zivi_module_page;
        zivi_ks_page = false;
        zivi_dirty = 1;
    }
    if (pressed & (1u << B_SEQ)) {
        zivi_module_page = false;
        zivi_ks_page = !zivi_ks_page;
        zivi_dirty = 1;
    }
    if (zivi_ks_page) {
#ifdef ZIVI_FELUCCA_PANEL
        bool random = (buttons & (1u << B_FX)) != 0;
#else
        bool random = (buttons & (1u << B_ARP)) != 0;
#endif
        if (random != zivi.ks.random_held) {
            synth_ks_random(&zivi, random);
            zivi_dirty = 1;
        }
        if (pressed & (1u << B_REC)) {
            if (zivi.ks.mode == KS_RECORDING) ks_stop(&zivi.ks);
            else synth_ks_record(&zivi);
        }
        if (pressed & (1u << B_PLAY)) {
            if (zivi.ks.mode == KS_PLAYING) ks_stop(&zivi.ks);
            else ks_play(&zivi.ks);
        }
        if (pressed & (1u << B_HOME)) ks_stop(&zivi.ks);
        if (pressed & (1u << B_SAVE)) zivi_load_preset(3);
        if (pressed & (1u << B_LFO)) zivi.ks.loop = !zivi.ks.loop;
        if (pressed & (1u << B_EDIT)) zivi.ks.sequence_trigger = !zivi.ks.sequence_trigger;
        if (pressed) zivi_dirty = 1;

        int real = plat_enc(EN_K1), sequence = plat_enc(EN_K2), time = plat_enc(EN_K3);
        int speed = plat_enc(EN_K4), transpose = plat_enc(EN_SELECT);
#ifdef ZIVI_FELUCCA_PANEL
        if (pressed & (1u << B_RIGHT)) synth_ks_matrix(&zivi, !zivi.ks_matrix);
        int dest = plat_enc(EN_PRESET), amount = plat_enc(EN_ALGO);
        if (dest) {
            int next = (int)zivi_ks_dest + dest % MATRIX_SIZE;
            zivi_ks_dest = (unsigned)(next + MATRIX_SIZE) % MATRIX_SIZE;
        }
        if (amount && !zivi.ks_matrix) {
            unsigned old = zivi.ks_routes[zivi_ks_dest];
            int depth = old == PIN_FULL ? 2 : (old == PIN_HALF ? 1 : 0);
            depth = clamp(depth + amount % 101, 0, 2);
            synth_ks_route(&zivi, zivi_ks_dest, depth == 2 ? PIN_FULL : (depth == 1 ? PIN_HALF : PIN_OFF));
        }
        if (dest || amount || (pressed & (1u << B_RIGHT))) zivi_dirty = 1;
#else
        plat_enc(EN_PRESET);
        plat_enc(EN_ALGO);
#endif
        if (real || sequence || time || speed || transpose) {
            ks_controls(&zivi.ks, (int)zivi.ks.realtime_spread + real % 101 * 5,
                        (int)zivi.ks.sequence_spread + sequence % 101 * 5,
                        (int)zivi.ks.speed + speed % 101 * 5, zivi.ks.transpose + transpose % 101,
                        (int)zivi.ks.limit_seconds + time % 101, zivi.ks.loop, zivi.ks.sequence_trigger);
            zivi_dirty = 1;
        }
    } else if (zivi.ks.random_held) {
        synth_ks_random(&zivi, false);
    }

    if (zivi_module_page) {
#ifdef ZIVI_FELUCCA_PANEL
        rack_input(pressed);
#else
        int select = plat_enc(EN_SELECT);
        int change = plat_enc(EN_K1);
        int preset = plat_enc(EN_PRESET);
        plat_enc(EN_ALGO);
        plat_enc(EN_K2);
        plat_enc(EN_K3);
        plat_enc(EN_K4);
        if (pressed & (1u << B_FX)) {
            zivi_scope = !zivi_scope;
            zivi_scope_frozen = false;
            zivi_trace_count = 0;
        }
        if (zivi_scope && (pressed & (1u << B_EDIT))) zivi_scope_frozen = !zivi_scope_frozen;
#ifndef ZIVI_FELUCCA_PANEL
        if (pressed & (1u << B_ARP)) synth_manual_attack(&zivi);
#endif
        if (pressed & (1u << B_HOME)) zivi_load_preset(0);
        if (pressed & (1u << B_SAVE)) zivi_load_preset(1);
        if (preset) {
            const int count = 4 + (int)synth_factory_count();
            int index = (zivi_preset < 0 ? 0 : zivi_preset) + preset % count + count;
            zivi_load_preset(index % count);
        }
        if (!zivi_scope && select) {
            zivi_param = (unsigned)((int)zivi_param + select % M4_COUNT + M4_COUNT) % M4_COUNT;
        }
        if (!zivi_scope && change) {
            synth_module_control(&zivi, zivi_param, zivi.modules.p[zivi_param] + module_params[zivi_param].step * change);
            zivi_preset = -1;
        }
        if (select || change || pressed) zivi_dirty = 1;
#endif
    }

    if (!zivi_ks_page && !zivi_module_page && (pressed & (1u << B_FX))) {
        zivi_zoom = !zivi_zoom;
        zivi_dirty = 1;
    }
    if (!zivi_ks_page && !zivi_module_page && (pressed & (1u << B_ENV))) {
        zivi_advanced = !zivi_advanced;
        zivi_dirty = 1;
    }
    if (!zivi_ks_page && !zivi_module_page && (pressed & (1u << B_HOME))) {
        zivi_load_preset(0);
        zivi_row = SRC_ENVELOPE;
        zivi_col = DST_OUT1_IN;
        zivi_pin_type = PIN_FULL;
        zivi_dirty = 1;
    }

    int row_move = (zivi_ks_page || zivi_module_page) ? 0 : plat_enc(EN_PRESET);
    int col_move = (zivi_ks_page || zivi_module_page) ? 0 : plat_enc(EN_SELECT);
    int type_move = (zivi_ks_page || zivi_module_page) ? 0 : plat_enc(EN_ALGO);
    if (row_move || col_move) {
        zivi_row = (unsigned)((int)zivi_row + row_move % MATRIX_SIZE + MATRIX_SIZE) % MATRIX_SIZE;
        zivi_col = (unsigned)((int)zivi_col + col_move % MATRIX_SIZE + MATRIX_SIZE) % MATRIX_SIZE;
        zivi_dirty = 1;
    }
    if (type_move % 2) {
        zivi_pin_type = zivi_pin_type == PIN_FULL ? PIN_HALF : PIN_FULL;
        zivi_dirty = 1;
    }
    if (!zivi_ks_page && !zivi_module_page && (pressed & (1u << B_EDIT))) {
        if (synth_toggle_pin(&zivi, zivi_row, zivi_col, zivi_pin_type)) {
            zivi_preset = -1;
            zivi_dirty = 1;
        }
    }

#ifndef ZIVI_PRO_BOARD
    for (unsigned i = 0; i < NKEYS && key_changes; ++i) {
        uint32_t bit = (uint32_t)1 << i;
        if (!(key_changes & bit)) continue;
        key_changes &= ~bit;
        if (!(keys & bit) && (zivi_pad_keys & bit)) {
            zivi_pad_keys &= ~bit;
        } else if (!zivi_ks_page && !zivi_module_page && (keys & bit) && (buttons & (1u << B_SEL)) && i < MATRIX_SIZE) {
            zivi_pad_keys |= bit;
            zivi_row = (zivi_row & ~3u) + i / MATRIX_TILE;
            zivi_col = (zivi_col & ~3u) + i % MATRIX_TILE;
            if (synth_toggle_pin(&zivi, zivi_row, zivi_col, zivi_pin_type)) zivi_preset = -1;
            zivi_zoom = true;
        } else {
            synth_midi(&zivi, 0x90, 53 + i, keys & bit ? 106 : 0);
        }
        zivi_dirty = 1;
    }

#endif

    if (zivi_ks_page || zivi_module_page) return;
    plat_enc(EN_K4);
    int k1 = plat_enc(EN_K1);
    int k2 = plat_enc(EN_K2);
    int k3 = plat_enc(EN_K3);
    if (k1 || k2 || k3) {
        zivi_preset = -1;
        if (zivi_advanced) {
            synth_patch_controls(&zivi, zivi.mix + k1 * 0.025,
                                 zivi.lfo * exp2(k2 / 8.0), zivi.cutoff * exp2(k3 / 12.0));
        } else {
            synth_controls(&zivi, zivi.tune + k1, zivi.shape + k2 * 0.025, zivi.level + k3 * 0.025);
        }
        zivi_dirty = 1;
    }
}

static void zivi_knob(int cx, int cy, int radius, uint16_t hue, ZiviReal position)
{
#ifdef ZIVI_FELUCCA_PANEL
    rack_knob(cx, cy, radius, hue, fmax(0.0f, fmin(1.0f, position)));
#else
    ZiviReal turn = (fmax(0.0, fmin(1.0, position)) - 0.5) * 4.71238898038469;
    for (int tick = 0; tick <= 10; ++tick) {
        ZiviReal angle = (tick / 10.0 - 0.5) * 4.71238898038469;
        ZiviReal sx = sin(angle), sy = -cos(angle);
        cv_line(cx + (int)lround(sx * (radius + 2)), cy + (int)lround(sy * (radius + 2)),
                cx + (int)lround(sx * (radius + 4)), cy + (int)lround(sy * (radius + 4)),
                angle <= turn ? hue : GRAPHITE);
    }
    for (int dy = -radius; dy <= radius; ++dy) {
        int span = (int)sqrt(radius * radius - dy * dy);
        cv_rect(cx - span, cy + dy, span * 2 + 1, 1, INK);
    }
    cv_line(cx, cy, cx + (int)lround(sin(turn) * (radius - 2)),
            cy - (int)lround(cos(turn) * (radius - 2)), WHITE);
#endif
}

static void zivi_draw_dial(int x, const char *label, const char *value, uint16_t hue, ZiviReal position)
{
    zivi_knob(x + 15, 200, 9, hue, position);
    cv_text(x + 29, 189, &FONT_T, label, INK);
    cv_text(x + 29, 200, &FONT_XS, value, INK);
}

static void zivi_draw_digit(int x, int y, unsigned digit)
{
    static const uint8_t glyphs[10][5] = {
        {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7},
        {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 2, 2, 2},
        {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}
    };
    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 3; ++col) {
            if (glyphs[digit][row] & (4u >> col)) cv_pset(x + col, y + row, WHITE);
        }
    }
}

#ifdef ZIVI_FELUCCA_PANEL
/* The width encodes strength. The cursor has its own outline. */
static void zivi_pin_mark(int x, int y, int size, unsigned pin, uint16_t surface, uint16_t empty)
{
    int half = size / 2;
    if (pin == PIN_FULL) {
        cv_rect(x - half + 1, y - half, size - 2, size, WHITE);
        cv_rect(x - half, y - half + 1, size, size - 2, WHITE);
    } else if (pin == PIN_HALF) {
        int height = size >= 10 ? 3 : 2;
        cv_rect(x - half, y - height / 2, size, height, PANEL_GREEN);
    } else {
        cv_rect(x - half, y - half, size, size, empty);
        cv_rect(x - half + 1, y - half + 1, size - 2, size - 2, surface);
    }
}
#endif

static void zivi_draw_matrix(void)
{
    unsigned first_row = zivi_zoom ? zivi_row & ~3u : 0;
    unsigned first_col = zivi_zoom ? zivi_col & ~3u : 0;
    unsigned size = zivi_zoom ? MATRIX_TILE : MATRIX_SIZE;
    int spacing_y = zivi_zoom ? 31 : 8;
#ifdef ZIVI_FELUCCA_PANEL
    int spacing_x = zivi_zoom ? 33 : 9;
    int dot = zivi_zoom ? 12 : 4;
    int start_x = zivi_zoom ? 39 : 23;
    cv_rect(6, 34, 159, 149, BLACK);
    cv_line(6, 34, 164, 34, GRAPHITE);
    cv_line(6, 182, 164, 182, GRAPHITE);
    if (!zivi_zoom) cv_rect(start_x + spacing_x - 4, 48, spacing_x, 128, DARK_BLUE);
#else
    int spacing_x = spacing_y;
    int dot = zivi_zoom ? 10 : 4;
    int start_x = zivi_zoom ? 38 : 26;
    cv_rect(6, 34, 149, 149, BLACK);
#endif
    int start_y = zivi_zoom ? 61 : 51;
    if (!zivi_zoom) {
        cv_rect(start_x - 3 + (int)(zivi_col & ~3u) * spacing_x,
                47 + (int)(zivi_row & ~3u) * spacing_y,
                spacing_x * MATRIX_TILE, spacing_y * MATRIX_TILE, MATRIX_TILE_COLOR);
#ifdef ZIVI_FELUCCA_PANEL
        for (int group = 1; group < MATRIX_TILE; ++group) {
            int x = start_x - 4 + group * 4 * spacing_x;
            int y = start_y - 4 + group * 4 * spacing_y;
            cv_line(x, 47, x, 175, PANEL_GRID);
            cv_line(19, y, 162, y, PANEL_GRID);
        }
#endif
    }
    for (unsigned j = 0; j < size; ++j) {
        bool driven = zivi.counts[first_col + j] != 0;
        const char legend[] = {(char)('A' + first_col + j), 0};
        int x = start_x + (int)j * spacing_x;
        uint16_t axis_color = driven ? AMBER : WHITE;
#ifdef ZIVI_FELUCCA_PANEL
        if (first_col + j == DST_METER) axis_color = PANEL_CYAN;
#endif
        cv_text(x - text_w(&FONT_T, legend) / 2, 36, &FONT_T, legend, axis_color);
        if (driven) cv_rect(x - 2, 46, 4, 1, AMBER);
    }
    for (unsigned i = 0; i < size; ++i) {
        unsigned row = first_row + i;
        unsigned label = row + 1;
        int y = start_y + (int)i * spacing_y;
        if (label >= 10) zivi_draw_digit(9, y - 2, label / 10);
        zivi_draw_digit(13, y - 2, label % 10);
        for (unsigned j = 0; j < size; ++j) {
            unsigned col = first_col + j;
            int x = start_x + (int)j * spacing_x;
            unsigned pin = zivi.matrix[row][col];
            if (!synth_pin_supported(row, col)) cv_rect(x - 2, y, 4, 1, GRAPHITE);
#ifdef ZIVI_FELUCCA_PANEL
            else {
                bool region = !zivi_zoom && row / 4 == zivi_row / 4 && col / 4 == zivi_col / 4;
                uint16_t surface = region ? MATRIX_TILE_COLOR : !zivi_zoom && col == DST_METER ? DARK_BLUE : BLACK;
                zivi_pin_mark(x, y, dot, pin, surface, SILVER);
            }
#else
            else if (pin == PIN_HALF) cv_rect(x - dot / 3, y - dot / 3, dot * 2 / 3, dot * 2 / 3, WHITE);
            else {
                cv_rect(x - dot / 2, y - dot / 2, dot, dot, pin == PIN_FULL ? AMBER : WHITE);
                if (pin == PIN_OFF) cv_rect(x - dot / 2 + 1, y - dot / 2 + 1, dot - 2, dot - 2, BLACK);
            }
#endif
            if (row == zivi_row && col == zivi_col) {
                int edge = zivi_zoom ? dot / 2 + 5 : 4;
#ifdef ZIVI_FELUCCA_PANEL
                uint16_t focus = PANEL_CYAN;
#else
                uint16_t focus = BLUE;
#endif
                cv_rect(x - edge, y - edge, 2 * edge, 1, focus);
                cv_rect(x - edge, y + edge - 1, 2 * edge, 1, focus);
                cv_rect(x - edge, y - edge, 1, 2 * edge, focus);
                cv_rect(x + edge - 1, y - edge, 1, 2 * edge, focus);
            }
        }
    }
}

static void zivi_header(const char *section)
{
    cv_begin(240, 240, IVORY);
#ifdef ZIVI_FELUCCA_PANEL
    cv_rect(0, 0, 240, 27, BLACK);
    cv_rect(0, 27, 240, 1, GRAPHITE);
    cv_rect(8, 7, 2, 12, AMBER);
    cv_text(15, 4, &FONT_B, APP_NAME, WHITE);
    int left = 230 - text_w(&FONT_T, section);
    cv_rect(left - 8, 8, 1, 11, BLUE);
    cv_text(left, 8, &FONT_T, section, WHITE);
#elif defined(ZIVI_OPEN_PANEL)
    cv_rect(0, 0, 240, 28, BLACK);
    cv_text(8, 6, &FONT_B, APP_NAME, WHITE);
    cv_text(232 - text_w(&FONT_T, section), 10, &FONT_T, section, AMBER);
    cv_rect(0, 28, 240, 2, BLUE);
#else
    cv_rect(0, 0, 240, 4, BLUE);
    cv_text(9, 7, &FONT_B, APP_NAME, INK);
    cv_text(231 - text_w(&FONT_T, section), 12, &FONT_T, section, INK);
    cv_rect(0, 29, 240, 1, INK);
#endif
}

static void zivi_footer(const char *hint)
{
#ifdef ZIVI_FELUCCA_PANEL
    cv_rect(0, 216, 240, 1, GRAPHITE);
    cv_rect(0, 217, 240, 23, BLACK);
#elif defined(ZIVI_OPEN_PANEL)
    cv_rect(0, 217, 240, 23, DARK_BLUE);
#else
    cv_rect(0, 217, 240, 23, BLUE);
#endif
    cv_text(8, 222, &FONT_XS, hint, WHITE);
}

#ifdef ZIVI_FELUCCA_PANEL
static void zivi_footer_pair(const char *left, const char *right)
{
    cv_rect(0, 216, 240, 1, GRAPHITE);
    cv_rect(0, 217, 240, 23, BLACK);
    int right_x = 231 - text_w(&FONT_T, right);
    cv_text(9, 223, &FONT_T, left, WHITE);
    cv_text(right_x, 223, &FONT_T, right, AMBER);
}
#endif

static void zivi_draw_ks(void)
{
#ifdef ZIVI_FELUCCA_PANEL
    zivi_header("SEQ / KS");
#else
    zivi_header("KS SEQUENCER");
#endif
    cv_rect(7, 37, 226, 56, BLACK);
    const char *mode = zivi.ks.mode == KS_RECORDING ? "RECORD"
                     : zivi.ks.mode == KS_PLAYING ? "PLAY" : "STOP";
    if (zivi.ks.mode == KS_RECORDING && !zivi.ks.count) mode = "ARMADO";
    uint16_t mode_color = zivi.ks.mode == KS_RECORDING ? AMBER : WHITE;
    cv_text(13, 44, &FONT_S, mode, mode_color);
    char value[24];
    snprintf(value, sizeof value, "%u / 256", zivi.ks.count);
    cv_text(156, 45, &FONT_XS, value, WHITE);
#ifdef ZIVI_FELUCCA_PANEL
    uint32_t duration = zivi.ks.mode == KS_RECORDING ? zivi.ks.record_limit : zivi.ks.length;
    if (duration) {
        for (unsigned i = 0; i < zivi.ks.count; ++i) {
            const KsEvent *event = &zivi.ks.events[i];
            if (!event->velocity) continue;
            uint32_t end = i + 1 < zivi.ks.count ? zivi.ks.events[i + 1].frame
                : zivi.ks.mode == KS_RECORDING ? zivi.ks.record_frame : duration;
            unsigned x = event->frame * 208u / duration;
            unsigned stop = end * 208u / duration;
            if (x >= 208) continue;
            if (stop > 208) stop = 208;
            cv_rect(16 + (int)x, 72 - (int)(event->note % 12u), stop > x ? (int)(stop - x) : 1, 1, WHITE);
        }
    }
#endif
    cv_rect(14, 77, 212, 6, GRAPHITE);
    cv_rect(14, 77, (int)zivi_ks_progress, 6, AMBER);
    if (zivi.ks.limit != KS_LIMIT_NONE) {
        cv_text(67, 46, &FONT_T, zivi.ks.limit == KS_LIMIT_MEMORY ? "MEM LIMIT" : "TIME LIMIT", AMBER);
    }
    cv_text(12, 101, &FONT_XS, zivi.ks.loop ? "LOOP ON" : "LOOP OFF", INK);
    cv_text(111, 101, &FONT_XS, zivi.ks.sequence_trigger ? "TRIGGER KS" : "TRIGGER KEYS", INK);
    snprintf(value, sizeof value, "REALTIME SPREAD %u%%", zivi.ks.realtime_spread);
    cv_text(12, 123, &FONT_XS, value, INK);
    snprintf(value, sizeof value, "SEQUENCE SPREAD %u%%", zivi.ks.sequence_spread);
    cv_text(12, 139, &FONT_XS, value, INK);
    snprintf(value, sizeof value, "LENGTH %u s", zivi.ks.limit_seconds);
    cv_text(12, 155, &FONT_XS, value, INK);
    if (zivi.ks.clock_speed_q8)
        snprintf(value, sizeof value, "EXT %u%%", (unsigned)(zivi.ks.clock_speed_q8 + 128u) >> 8);
    else snprintf(value, sizeof value, "SPEED %u%%", zivi.ks.speed);
    cv_text(111, 155, &FONT_XS, value, INK);
    snprintf(value, sizeof value, "TRANSPOSE %+d st", zivi.ks.transpose);
    cv_text(12, 171, &FONT_XS, value, INK);
#ifdef ZIVI_FELUCCA_PANEL
    const char *amounts[] = {"OFF", "100%", "50%"};
    if (zivi.ks_matrix)
        cv_text(12, 194, &FONT_T, "EMS: KS ON ROW 16", INK);
    else {
        snprintf(value, sizeof value, "KS %c %s", 'A' + (int)zivi_ks_dest, amounts[zivi.ks_routes[zivi_ks_dest]]);
        cv_text(12, 194, &FONT_T, value, INK);
        cv_text(91, 194, &FONT_T, synth_dest_name(zivi_ks_dest), INK);
    }
#else
    cv_text(12, 195, &FONT_T, "ARP RANDOM   SAVE KS PATCH", INK);
#endif
#ifdef ZIVI_FELUCCA_PANEL
    zivi_footer("< STOP   > EMS   K5 DEST  K7 AMT");
#elif defined(ZIVI_PRO_BOARD)
    zivi_footer("REC   PLAY   LEFT STOP   SEQ EXIT");
#else
    zivi_footer("REC    PLAY    HOME STOP    SEQ MATRIX");
#endif
    cv_blit(0, 0);
}

static void zivi_frequency(int x, int y, unsigned index)
{
    static const char *const labels[] = {"OSC 1", "OSC 2", "OSC 3", "CUTOFF"};
    static const unsigned cols[] = {DST_OSC1_PITCH, DST_OSC2_PITCH, DST_OSC3_PITCH, DST_FILTER_CUTOFF};
    char value[20];
    ZiviReal hz = zivi.hz[index];
    if (hz > 0) snprintf(value, sizeof value, hz < 10 ? "%.2f Hz" : "%.0f Hz", ZIVI_LABEL(hz));
    else snprintf(value, sizeof value, "- Hz");
    const char *marker = zivi.counts[cols[index]] ? "*" : "";
    cv_rect(x, y, 109, 24, BLACK);
    cv_text(x + 4, y + 7, &FONT_T, labels[index], WHITE);
    cv_text(x + 41, y + 5, &FONT_XS, marker, AMBER);
    cv_text(x + 104 - text_w(&FONT_XS, value), y + 5, &FONT_XS, value, AMBER);
}

static void zivi_draw_scope(void)
{
    zivi_header("SCOPE");
    cv_text(9, 36, &FONT_XS, zivi_scope_frozen ? "HOLD" : "TRACE 11.6 ms", INK);
    int maximum = 0;
    unsigned oldest = (zivi_trace_head + TRACE_POINTS - zivi_trace_count) % TRACE_POINTS;
    for (unsigned i = 0; i < zivi_trace_count; ++i) {
        int sample = zivi_trace[(oldest + i) % TRACE_POINTS];
        int magnitude = sample < 0 ? -sample : sample;
        if (magnitude > maximum) maximum = magnitude;
    }
    int scale = maximum <= 328 ? 328 : maximum <= 1638 ? 1638 : 8192;
    const char *range = scale == 328 ? "+/- 0.02" : scale == 1638 ? "+/- 0.1" : "+/- 0.5";
    cv_text(230 - text_w(&FONT_T, range), 36, &FONT_T, range, INK);
    cv_rect(8, 54, 224, 80, BLACK);
    for (int x = 8; x < 232; x += 28) cv_rect(x, 54, 1, 80, GRAPHITE);
    cv_rect(8, 94, 224, 1, GRAPHITE);
    if (zivi_trace_count > 1) {
        int prior_y = 94 - 38 * zivi_trace[oldest] / scale;
        int prior_x = 9;
        for (unsigned i = 1; i < zivi_trace_count; ++i) {
            int x = 9 + (int)(221 * i / (TRACE_POINTS - 1));
            int y = 94 - 38 * zivi_trace[(oldest + i) % TRACE_POINTS] / scale;
            cv_line(prior_x, prior_y, x, y, AMBER);
            prior_x = x;
            prior_y = y;
        }
    }
    for (unsigned i = 0; i < 4; ++i) {
        zivi_frequency(8 + (int)(i % 2) * 114, 143 + (int)(i / 2) * 28, i);
    }
#ifdef ZIVI_PRO_BOARD
    zivi_footer("FX BACK   PARA HOLD   GLOBE EXIT");
#else
    zivi_footer("FX MODULES   EDIT HOLD   GLO MATRIX");
#endif
    cv_blit(0, 0);
}

#ifndef ZIVI_FELUCCA_PANEL
static void zivi_envelope_graph(void)
{
    static const char *const phases[] = {"OFF", "ATTACK", "ON", "DECAY"};
    const ZiviReal *p = zivi.modules.p;
    unsigned mode = (unsigned)p[M4_ENV_MODE];
    ZiviReal attack = p[M4_ATTACK_MS], on = p[M4_ON_MS], decay = p[M4_DECAY_MS];
    ZiviReal off = mode == 2 ? p[M4_OFF_MS] : 0;
    if (mode == 0) {
        on = 15 * attack;
        decay *= 5;
    }
    ZiviReal release = attack + on;
    ZiviReal duration = release + decay + off;
    cv_rect(8, 147, 224, 66, BLACK);
    cv_text(14, 151, &FONT_T, phases[zivi.modules.phase], AMBER);
    char text[24];
    snprintf(text, sizeof text, mode == 0 ? "GATE  ~%.0f ms" : "%.0f ms", ZIVI_LABEL(duration));
    cv_text(227 - text_w(&FONT_T, text), 151, &FONT_T, text, WHITE);
    cv_rect(12, 205, 216, 1, GRAPHITE);
    const ZiviReal transition[] = {attack, release, release + decay};
    for (unsigned i = 0; i < 3; ++i) {
        int x = 12 + (int)(215 * transition[i] / duration);
        cv_rect(x, 164, 1, 42, GRAPHITE);
    }
    int prior = 205;
#ifdef ZIVI_OPEN_PANEL
    const int stride = 5;
#else
    const int stride = 1;
#endif
    for (int x = stride; x <= 215; x += stride) {
        ZiviReal t = x * duration / 215;
        ZiviReal level;
        if (mode == 0) {
            level = t < release ? 1 - exp(-t / attack)
                               : (1 - exp(-release / attack)) * exp(-5 * (t - release) / decay);
        } else {
            level = t < attack ? t / attack : t < release ? 1.0 : fmax(0.0, 1 - (t - release) / decay);
        }
        int y = 205 - (int)lround(40 * level);
        cv_line(12 + x - stride, prior, 12 + x, y, AMBER);
        prior = y;
    }
}

static void zivi_draw_modules(void)
{
    static const char *const groups[] = {"OSCILLATOR 1", "OSCILLATOR 2", "OSCILLATOR 3",
        "NOISE / RING MOD", "FILTER / OSCILLATOR", "ENVELOPE SHAPER", "SPRING REVERB", "INPUT / OUTPUT"};
    static const char *const modes[] = {"GATE", "SINGLE", "REPEAT"};
    const ModuleParamInfo *param = &module_params[zivi_param];
#if defined(ZIVI_PRO_BOARD) && !defined(ZIVI_OPEN_PANEL)
    char header[24];
    snprintf(header, sizeof header, "PATCH  %u/%u", preset_page + 1, preset_pages());
    zivi_header(header);
#else
    zivi_header("MODULES");
#endif
    cv_text(9, 35, &FONT_XS, groups[param->group], INK);
    cv_line(8, 53, 232, 53, GRAPHITE);
    ZiviReal number = zivi.modules.p[zivi_param];
    char value[24];
    if (zivi_param == M4_ENV_MODE) snprintf(value, sizeof value, "%s", modes[(unsigned)number]);
    else if (zivi_param == M4_TRIGGER_MODE) snprintf(value, sizeof value, "%s", number ? "C LEVEL" : "KEY / KS");
    else snprintf(value, sizeof value, "%.2f", ZIVI_LABEL(number));
    cv_rect(8, 59, 224, 70, SILVER);
    ZiviReal turn = (number - param->lo) / (param->hi - param->lo);
    if (zivi_param == M4_OSC1_HZ || zivi_param == M4_OSC2_HZ) {
        turn = log(number / param->lo) / log(param->hi / param->lo);
    }
    zivi_knob(44, 94, 25, param->group == 5 ? RED : BLUE, turn);
    cv_text(80, 76, &FONT_XS, param->label, INK);
    cv_text(80, 94, &FONT_B, value, INK);
    const char *preset = zivi_preset < 0 ? "PATCH EDITED" : zivi_preset < 4 ? default_presets[zivi_preset]
                                      : synth_factory_display((unsigned)zivi_preset - 4);
    cv_text(10, 132, &FONT_T, preset, INK);
    snprintf(value, sizeof value, "%02u / %u", zivi_param + 1, M4_COUNT);
    cv_text(230 - text_w(&FONT_T, value), 132, &FONT_T, value, INK);
    if (param->group == 5) zivi_envelope_graph();
    else {
        for (unsigned i = 0; i < 4; ++i) {
            zivi_frequency(8 + (int)(i % 2) * 114, 153 + (int)(i / 2) * 28, i);
        }
    }
#if defined(ZIVI_OPEN_PANEL)
    zivi_footer("K6 PARAM  K1 VAL  FX SCOPE");
#elif defined(ZIVI_PRO_BOARD)
    zivi_footer("LEFT/RIGHT BANK   K6 SEL   K1 VAL");
#else
#ifdef ZIVI_FELUCCA_PANEL
    zivi_footer("FX SCOPE   REC ATTACK   ARP MODULO");
#else
    zivi_footer("PRESET   SELECT   K1   FX SCOPE   ARP");
#endif
#endif
    cv_blit(0, 0);
}

#endif

static void zivi_draw(void)
{
    if (zivi_module_page) {
        if (zivi_scope) zivi_draw_scope();
#ifdef ZIVI_FELUCCA_PANEL
        else rack_draw();
#else
        else zivi_draw_modules();
#endif
        return;
    }
    if (zivi_ks_page) {
        zivi_draw_ks();
        return;
    }
    zivi_header(zivi_zoom ? "EDIT 4 X 4" : "OVERVIEW");
    zivi_draw_matrix();

    char value[22];
#ifdef ZIVI_FELUCCA_PANEL
    const int route_x = 168;
#else
    const int route_x = 157;
#endif
    cv_rect(route_x, 34, 234 - route_x, 149, SILVER);
    snprintf(value, sizeof value, "%c%u", (int)('A' + zivi_col), zivi_row + 1);
    cv_text(route_x + 5, 38, &FONT_B, value, INK);
    cv_text(route_x + 5, 59, &FONT_T, zivi_row == SRC_STICK_Y && zivi.ks_matrix ? "KS PITCH" : synth_source_name(zivi_row), INK);
    cv_text(route_x + 5, 72, &FONT_T, synth_dest_name(zivi_col), INK);
    unsigned active = zivi.matrix[zivi_row][zivi_col];
    const char *state = active == PIN_FULL ? "PINO 100%" : active == PIN_HALF ? "PINO 50%" : "SEM PINO";
    if (!synth_pin_supported(zivi_row, zivi_col)) state = "INATIVO";
#ifdef ZIVI_FELUCCA_PANEL
    cv_rect(route_x + 3, 84, 228 - route_x, 15, BLACK);
    zivi_pin_mark(route_x + 9, 91, 6, active, BLACK, SILVER);
    cv_text(route_x + 16, 85, &FONT_T, state, WHITE);
#else
    cv_text(route_x + 5, 86, &FONT_T, state, INK);
#endif
    if (synth_pin_delayed(zivi_row, zivi_col)) cv_text(route_x + 5, 99, &FONT_T, "ATRASO 1", INK);
    for (unsigned i = 0; i < MATRIX_TILE; ++i) {
        for (unsigned j = 0; j < MATRIX_TILE; ++j) {
            uint16_t color = i == zivi_row / MATRIX_TILE && j == zivi_col / MATRIX_TILE ? BLUE : GRAPHITE;
            cv_rect(route_x + 5 + (int)j * 5, 113 + (int)i * 5, 4, 4, color);
        }
    }
#ifdef ZIVI_FELUCCA_PANEL
    cv_text(route_x + 28, 109, &FONT_T, "K7 PINO", GRAPHITE);
    cv_text(route_x + 28, 121, &FONT_T, zivi_pin_type == PIN_FULL ? "100%" : "50%", INK);
#else
    cv_text(190, 118, &FONT_T, zivi_pin_type == PIN_FULL ? "100%" : "50%", INK);
#endif
#ifdef ZIVI_OPEN_PANEL
    cv_text(route_x + 5, 135, &FONT_T, "L", INK);
    cv_text(route_x + 5, 150, &FONT_T, "R", INK);
    cv_text(route_x + 5, 165, &FONT_T, "B", INK);
#ifdef ZIVI_FELUCCA_PANEL
    zivi_monitor_bus_drawn = zivi_monitor_bus;
#endif
    const unsigned meters[] = {zivi_peak, zivi_monitor, zivi_monitor_bus};
    for (unsigned i = 0; i < 3; ++i) {
        int y = 137 + (int)i * 15;
        int value = (int)(meters[i] > 53 ? 53 : meters[i]);
        const int left = route_x + 17, width = 228 - left;
#ifdef ZIVI_FELUCCA_PANEL
        const uint16_t color = i == 2 ? PANEL_CYAN : AMBER;
#else
        const uint16_t color = AMBER;
#endif
        cv_rect(left, y, width, 4, INK);
        cv_rect(left, y, value * width / 53, 4, color);
    }
#else
    cv_text(162, 137, &FONT_T, "OUT", INK);
    cv_rect(162, 151, 67, 5, INK);
    cv_rect(163, 152, zivi_peak > 32 ? 65 : (int)zivi_peak * 2, 3, AMBER);
    cv_text(162, 160, &FONT_T, "MONITOR B", INK);
    cv_rect(162, 174, 67, 5, INK);
    cv_rect(163, 175, zivi_monitor > 65 ? 65 : (int)zivi_monitor, 3, AMBER);
#endif

#ifdef ZIVI_FELUCCA_PANEL
    const uint16_t shape_color = GREEN, level_color = WHITE;
    const uint16_t osc3_color = BLUE, cutoff_color = BLUE, mix_color = GREEN;
#else
    const uint16_t shape_color = RED, level_color = GREEN;
    const uint16_t osc3_color = RED, cutoff_color = GREEN, mix_color = BLUE;
#endif
    if (zivi_advanced) {
        snprintf(value, sizeof value, "%.0f%%", ZIVI_LABEL(zivi.mix * 100));
        zivi_draw_dial(5, "MIX", value, mix_color, zivi.mix);
        snprintf(value, sizeof value, "%.1f", ZIVI_LABEL(zivi.lfo));
        zivi_draw_dial(83, "OSC 3", value, osc3_color, log(zivi.lfo / 0.02) / log(500 / 0.02));
        snprintf(value, sizeof value, "%.0f", ZIVI_LABEL(zivi.cutoff));
        zivi_draw_dial(161, "CUTOFF", value, cutoff_color, log(zivi.cutoff / 5) / log(10000 / 5));
    } else {
        snprintf(value, sizeof value, "%+.0f st", ZIVI_LABEL(zivi.tune));
        zivi_draw_dial(5, "TUNE", value, BLUE, (zivi.tune + 24) / 48);
        snprintf(value, sizeof value, "%.0f%%", ZIVI_LABEL(zivi.shape * 100));
        zivi_draw_dial(83, "SHAPE", value, shape_color, zivi.shape);
        snprintf(value, sizeof value, "%.0f%%", ZIVI_LABEL(zivi.level * 100));
        zivi_draw_dial(161, "LEVEL", value, level_color, zivi.level);
    }
#if defined(ZIVI_FELUCCA_PANEL)
    if (!zivi_zoom && zivi_preset >= 4) {
        char preset_label[48];
        const unsigned index = (unsigned)zivi_preset - 4;
        if (index < FACTORY_PROGRAMS)
            snprintf(preset_label, sizeof preset_label, "K4 %u/%u  %s", index + 1,
                     FACTORY_PROGRAMS, synth_factory_display(index));
        else
            snprintf(preset_label, sizeof preset_label, "USB FX  %s", synth_factory_display(index));
        zivi_footer(preset_label);
    } else if (!zivi_zoom) zivi_footer("K4 PRESET   FX ZOOM   PARA PIN");
    else zivi_footer("FX ZOOM   PARA PIN   PADS 1-16");
#elif defined(ZIVI_PRO_BOARD)
    zivi_footer("FX ZOOM   PARA PIN   PADS 1-16");
#else
    zivi_footer("FX ZOOM   EDIT PINO   SEL + PADS");
#endif
    cv_blit(0, 0);
}

#ifndef ZIVI_FELUCCA_PANEL
void app_splash(void)
{
    cv_begin(240, 240, IVORY);
    cv_rect(76, 43, 88, 88, BLACK);
    for (int row = 0; row < 16; ++row) {
        for (int col = 0; col < 16; ++col) {
            cv_rect(82 + col * 5, 49 + row * 5, 2, 2, row == col ? AMBER : GRAPHITE);
        }
    }
    cv_text(120 - text_w(&FONT_B, APP_NAME) / 2, 145, &FONT_B, APP_NAME, INK);
#ifdef ZIVI_PRO_BOARD
    cv_text(65, 174, &FONT_XS, "SMK-37 PRO  " ZIVI_VERSION, INK);
#else
    cv_text(65, 174, &FONT_XS, "HOST  " ZIVI_VERSION, INK);
#endif
    cv_rect(0, 217, 240, 23, BLUE);
    cv_blit(0, 0);
}

#endif

void app_ui_init(void)
{
    zivi_draw();
}

static void zivi_update_frame(void)
{
    if (zivi_ks_page) {
        bool recording = zivi.ks.mode == KS_RECORDING;
        uint32_t end = recording ? zivi.ks.record_limit : zivi.ks.length;
        uint32_t at = recording ? zivi.ks.record_frame : zivi.ks.position / 100u;
        unsigned progress = end ? (at < end ? at : end) * 212u / end : 0;
        if (zivi_ks_progress != progress || zivi_ks_mode != zivi.ks.mode || zivi_ks_count != zivi.ks.count) {
            zivi_ks_progress = progress;
            zivi_ks_mode = zivi.ks.mode;
            zivi_ks_count = zivi.ks.count;
            zivi_dirty = 1;
        }
    }
#ifndef ZIVI_FELUCCA_PANEL
    if (zivi_module_page && !zivi_scope && zivi_phase != zivi.modules.phase) {
        zivi_phase = zivi.modules.phase;
        zivi_dirty = 1;
    }
#endif
    uint32_t time = plat_ms();
    if (zivi_module_page && time - zivi_last_visual >= TRACE_PERIOD_MS) {
        zivi_last_visual = time;
        if (zivi_scope && !zivi_scope_frozen) zivi_dirty = 1;
#ifndef ZIVI_FELUCCA_PANEL
        if (!zivi_scope && module_params[zivi_param].group != 5) {
            for (unsigned i = 0; i < 4; ++i) {
                bool precise = zivi.hz[i] < 10.0;
                unsigned reading = (unsigned)lround(zivi.hz[i] * (precise ? 100.0 : 1.0)) * 2 + precise;
                if (zivi_hz_display[i] != reading) zivi_dirty = 1;
                zivi_hz_display[i] = reading;
            }
        }
#endif
    }
    if (zivi_dirty) {
        zivi_draw();
        zivi_dirty = 0;
    }
}

void app_frame(void)
{
    app_input_only();
    zivi_update_frame();
}

void app_say(const char *first, const char *second)
{
    (void)first;
    (void)second;
}

#ifndef ZIVI_BSP_LIBC
int app_expect(const char *what, int *got)
{
    if (strcmp(what, "zmodule") == 0) *got = zivi_module_page;
    else if (strcmp(what, "zscope") == 0) *got = zivi_scope;
    else if (strcmp(what, "zhold") == 0) *got = zivi_scope_frozen;
    else if (strcmp(what, "ztrace") == 0) *got = (int)zivi_trace_count;
    else if (strcmp(what, "zpreset") == 0) *got = zivi_preset;
    else if (strcmp(what, "zhz") == 0) *got = (int)lround(zivi.hz[0]);
    else if (strcmp(what, "zparam") == 0) *got = (int)zivi_param;
    else if (strcmp(what, "zenvmode") == 0) *got = (int)zivi.modules.p[M4_ENV_MODE];
    else if (strcmp(what, "zksmode") == 0) *got = (int)zivi.ks.mode;
    else if (strcmp(what, "zksevents") == 0) *got = (int)zivi.ks.count;
    else if (strcmp(what, "zksnote") == 0) *got = (int)zivi.ks.note;
    else if (strcmp(what, "zksvel") == 0) *got = (int)zivi.ks.velocity;
    else if (strcmp(what, "zksgate") == 0) *got = zivi.ks.sequence_trigger;
    else if (strcmp(what, "zrow") == 0) *got = (int)zivi_row + 1;
    else if (strcmp(what, "zcol") == 0) *got = (int)zivi_col + 1;
    else if (strcmp(what, "zzoom") == 0) *got = zivi_zoom;
    else if (strcmp(what, "zmonitor") == 0) *got = (int)zivi_monitor;
    else if (strcmp(what, "zpin") == 0) {
        *got = zivi.matrix[zivi_row][zivi_col];
    } else if (strcmp(what, "znote") == 0) {
        *got = zivi.held_count ? zivi.held[zivi.held_count - 1] : -1;
    } else return 0;
    return 1;
}
#endif
