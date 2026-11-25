/* Physical encoder bindings and visual metadata share the catalog with Web Studio. */
enum RackPage {
#define RACK(id, name, tab, color) RACK_##id,
#define DIAL(page, key, param, label, color, format)
#include "../module_panel.def"
#undef DIAL
#undef RACK
    RACK_COUNT
};

enum RackHue { RACK_HUE_BLUE, RACK_HUE_GREEN, RACK_HUE_WHITE, RACK_HUE_YELLOW, RACK_HUE_RED };
enum RackFormat { RACK_FMT_HZ, RACK_FMT_PCT, RACK_FMT_ST, RACK_FMT_FACTOR, RACK_FMT_MS,
                  RACK_FMT_MODE, RACK_FMT_TRIGGER, RACK_FMT_SEC };
enum RackQuick { M4_QUICK_SHAPE = M4_COUNT, M4_QUICK_TUNE, M4_QUICK_MIX, M4_QUICK_LFO,
                 M4_QUICK_CUTOFF, M4_QUICK_LEVEL };

typedef struct {
    const char *label;
    uint8_t param, hue, format;
} RackDial;

static const struct {
    const char *name, *tab;
    uint8_t hue;
} rack_panels[] = {
#define RACK(id, name, tab, color) [RACK_##id] = {name, tab, RACK_HUE_##color},
#define DIAL(page, key, param, label, color, format)
#include "../module_panel.def"
#undef DIAL
#undef RACK
};

static const RackDial rack_controls[RACK_COUNT][8] = {
#define RACK(id, name, tab, color)
#define DIAL(page, key, param, label, color, format) \
    [RACK_##page][key - 1] = {label, M4_##param, RACK_HUE_##color, RACK_FMT_##format},
#include "../module_panel.def"
#undef DIAL
#undef RACK
};

static unsigned rack_page, rack_master_drawn;
static int rack_k8_delta;
static const uint16_t rack_palette[] = { BLUE, GREEN, WHITE, AMBER, RED };

static void rack_select(unsigned page)
{
    if (rack_page == page) return;
    rack_page = page;
    rack_k8_delta = 0;
    zivi_dirty = 1;
}

static bool rack_shows_master(void)
{
    return !rack_controls[rack_page][7].label;
}

static unsigned rack_master_percent(void)
{
    return (plat_master() * 100u + 2048u) / 4096u;
}

static const ModuleParamInfo rack_quick_specs[] = {
    {.lo = 0.0f, .hi = 1.0f, .step = 0.01f},
    {.lo = -24.0f, .hi = 24.0f, .step = 0.5f},
    {.lo = 0.0f, .hi = 1.0f, .step = 0.01f},
    {.lo = 0.02f, .hi = 500.0f, .step = 0.01f},
    {.lo = 5.0f, .hi = 10000.0f, .step = 1.0f},
    {.lo = 0.0f, .hi = 1.0f, .step = 0.01f},
};

static float rack_value(unsigned param)
{
    if (param < M4_COUNT) return zivi.modules.p[param];
    switch (param) {
    case M4_QUICK_SHAPE: return zivi.shape;
    case M4_QUICK_TUNE: return zivi.tune;
    case M4_QUICK_MIX: return zivi.mix;
    case M4_QUICK_LFO: return zivi.lfo;
    case M4_QUICK_CUTOFF: return zivi.cutoff;
    default: return zivi.level;
    }
}

typedef struct {
    float lo, hi, low_hz, high_hz, low_octave, slope, inverse;
} VernierSegment;
#include "../../build/felucca-vernier.inc"

static struct VernierPosition {
    float hz, position;
} vernier_positions[4];

static float rack_response_position(float response)
{
    const float middle = vernier_response_middle;
    return response <= middle ? 0.5f * response / middle : 0.5f + 0.5f * (response - middle) / (1.0f - middle);
}

static float rack_frequency(unsigned param, float hz, int delta, float *position)
{
    const VernierSegment *curve = vernier_audio;
    unsigned size = sizeof vernier_audio / sizeof *vernier_audio;
    unsigned slot = param == M4_OSC1_HZ ? 0 : 1;
    if (param == M4_QUICK_LFO) {
        curve = vernier_low;
        size = sizeof vernier_low / sizeof *vernier_low;
        slot = 2;
    } else if (param == M4_QUICK_CUTOFF) {
        curve = vernier_filter;
        size = sizeof vernier_filter / sizeof *vernier_filter;
        slot = 3;
    }

    struct VernierPosition *cached = &vernier_positions[slot];
    if (cached->hz != hz) {
        unsigned i = 0;
        while (i + 1 < size && hz > curve[i].high_hz) ++i;
        const VernierSegment *segment = &curve[i];
        cached->hz = hz;
        if (hz <= segment->low_hz) cached->position = segment->lo;
        else if (hz >= segment->high_hz) cached->position = segment->hi;
        else cached->position = segment->lo + (log(hz) * 1.44269504f - segment->low_octave) * segment->inverse;
    }
    *position = cached->position;
    if (!delta) return hz;
    float next = fmax(0.0f, fmin(1.0f, *position + delta / 720.0f));
    if (next == *position) return hz;

    unsigned i = 0;
    while (i + 1 < size && next > curve[i].hi) ++i;
    const VernierSegment *segment = &curve[i];
    *position = cached->position = next;
    cached->hz = next == segment->lo ? segment->low_hz : next == segment->hi ? segment->high_hz :
        segment->low_hz * exp2((next - segment->lo) * segment->slope);
    return cached->hz;
}

static void rack_turn(const RackDial *dial, int delta)
{
    if (!dial->label || !delta) return;
    const unsigned param = dial->param;
    const ModuleParamInfo *spec = param < M4_COUNT ? &module_params[param] : &rack_quick_specs[param - M4_COUNT];
    const float before = rack_value(param);
    const float step = dial->format == RACK_FMT_MS ? fmax(spec->step, before * 0.035f) : spec->step;
    delta = clamp(delta, -127, 127);
    float position;
    float value = before + step * delta;
    if (dial->format == RACK_FMT_HZ) value = rack_frequency(param, before, delta, &position);
    else if (param == M4_RESPONSE) {
        position = rack_response_position(before) + step * delta;
        const float middle = vernier_response_middle;
        value = position <= 0.5f ? 2.0f * position * middle : middle + 2.0f * (position - 0.5f) * (1.0f - middle);
    }
    value = fmin(spec->hi, fmax(spec->lo, value));
    if (value == before) return;
    switch (param) {
    case M4_QUICK_SHAPE: synth_controls(&zivi, zivi.tune, value, zivi.level); break;
    case M4_QUICK_TUNE: synth_controls(&zivi, value, zivi.shape, zivi.level); break;
    case M4_QUICK_LEVEL: synth_controls(&zivi, zivi.tune, zivi.shape, value); break;
    case M4_QUICK_MIX: synth_patch_controls(&zivi, value, zivi.lfo, zivi.cutoff); break;
    case M4_QUICK_LFO: synth_patch_controls(&zivi, zivi.mix, value, zivi.cutoff); break;
    case M4_QUICK_CUTOFF: synth_patch_controls(&zivi, zivi.mix, zivi.lfo, value); break;
    default: synth_module_control(&zivi, param, value); break;
    }
    zivi_preset = -1;
    zivi_dirty = 1;
}

static void rack_input(uint32_t pressed)
{
    if (pressed & (1u << B_FX)) {
        zivi_scope = !zivi_scope;
        zivi_scope_frozen = false;
        zivi_trace_count = 0;
    }
    if (zivi_scope && (pressed & (1u << B_EDIT))) zivi_scope_frozen = !zivi_scope_frozen;
    if (!zivi_scope) {
        if (pressed & (1u << B_LEFT)) rack_select((rack_page + RACK_COUNT - 1u) % RACK_COUNT);
        if (pressed & (1u << B_RIGHT)) rack_select((rack_page + 1u) % RACK_COUNT);
    }

    static const int roles[] = {EN_K1, EN_K2, EN_K3, EN_K4, EN_PRESET, EN_SELECT, EN_ALGO};
    for (unsigned i = 0; i < sizeof roles / sizeof *roles; ++i) {
        int delta = plat_enc(roles[i]);
        if (!zivi_scope) rack_turn(&rack_controls[rack_page][i], delta);
    }
    if (!zivi_scope) rack_turn(&rack_controls[rack_page][7], rack_k8_delta);
    rack_k8_delta = 0;
    if (pressed) zivi_dirty = 1;
}

/* Draw the bezel, colored cap and indicator at native LCD size. */
static void rack_knob(int cx, int cy, int radius, uint16_t color, float position)
{
    const float sweep = 4.712389f;
    const float turn = (position - 0.5f) * sweep;
    for (unsigned i = 0; i < 11; ++i) {
        float a = ((float)i / 10.0f - 0.5f) * sweep;
        int x = cx + (int)lround(sin(a) * (radius + 4));
        int y = cy - (int)lround(cos(a) * (radius + 4));
        cv_pset(x, y, i == 0 || i == 5 || i == 10 ? BLACK : GRAPHITE);
    }
    for (int y = -radius; y <= radius; ++y) {
        int edge = (int)sqrt(radius * radius - y * y);
        cv_rect(cx - edge, cy + y, edge * 2 + 1, 1, BLACK);
        if (edge > 2 && y > 1 - radius && y < radius - 2)
            cv_rect(cx - edge + 2, cy + y, edge * 2 - 3, 1, color);
    }
    uint16_t mark = color == WHITE || color == AMBER ? INK : WHITE;
    cv_line(cx + (int)lround(sin(turn) * 4), cy - (int)lround(cos(turn) * 4),
            cx + (int)lround(sin(turn) * (radius - 2)), cy - (int)lround(cos(turn) * (radius - 2)), mark);
}

static void rack_text_center(int x, int y, int width, const char *label, uint16_t color)
{
    cv_text(x + (width - text_w(&FONT_T, label)) / 2, y, &FONT_T, label, color);
}

static void rack_format(const RackDial *dial, float value, char *out, unsigned size)
{
    static const char *const modes[] = {"Gate", "Single", "Repeat"};
    switch (dial->format) {
    case RACK_FMT_HZ:
        if (value >= 1000.0f) snprintf(out, size, "%.1f kHz", ZIVI_LABEL(value * 0.001f));
        else if (value >= 10.0f) snprintf(out, size, "%.0f Hz", ZIVI_LABEL(value));
        else snprintf(out, size, "%.2f Hz", ZIVI_LABEL(value));
        break;
    case RACK_FMT_MS:
        if (value >= 1000.0f) snprintf(out, size, "%.1f s", ZIVI_LABEL(value * 0.001f));
        else snprintf(out, size, "%.0f ms", ZIVI_LABEL(value));
        break;
    case RACK_FMT_ST: snprintf(out, size, "%+.1f st", ZIVI_LABEL(value)); break;
    case RACK_FMT_FACTOR: snprintf(out, size, "%.1fx", ZIVI_LABEL(value)); break;
    case RACK_FMT_SEC: snprintf(out, size, "%.2f s", ZIVI_LABEL(value)); break;
    case RACK_FMT_MODE: snprintf(out, size, "%s", modes[(unsigned)value]); break;
    case RACK_FMT_TRIGGER: snprintf(out, size, "%s", value > 0.0f ? "Limit C" : "Key / KS"); break;
    default: snprintf(out, size, "%.0f%%", ZIVI_LABEL(value * 100.0f)); break;
    }
}

static void rack_draw(void)
{
    zivi_header("GLOBAL / MODULES");
    for (unsigned i = 0; i < RACK_COUNT; ++i) {
        int x = 7 + (int)(i % 5) * 46, y = 32 + (int)(i / 5) * 17;
        bool selected = i == rack_page;
        cv_rect(x, y, 42, 15, selected ? BLACK : SILVER);
        rack_text_center(x, y + 1, 42, rack_panels[i].tab, selected ? WHITE : INK);
        if (selected) cv_rect(x, y + 13, 42, 2, rack_palette[rack_panels[i].hue]);
    }
    cv_rect(7, 69, 226, 22, BLACK);
    cv_rect(7, 69, 3, 22, rack_palette[rack_panels[rack_page].hue]);
    cv_text(15, 71, &FONT_S, rack_panels[rack_page].name, WHITE);

    rack_master_drawn = rack_master_percent();
    for (unsigned i = 0; i < 8; ++i) {
        const RackDial *dial = &rack_controls[rack_page][i];
        int x = 7 + (int)(i % 4) * 57, y = 96 + (int)(i / 4) * 59;
        bool master = i == 7 && rack_shows_master();
        char key[3] = {'K', (char)('1' + i), 0};
        cv_rect(x, y, 54, 56, dial->label || master ? SILVER : IVORY);
        if (dial->label || master) {
            cv_rect(x, y + 55, 54, 1, GRAPHITE);
            cv_rect(x + 2, y + 44, 50, 11, IVORY);
        }
        cv_text(x + 2, y + 1, &FONT_T, key, dial->label || master ? INK : GRAPHITE);
        if (!dial->label && !master) {
            rack_text_center(x, y + 25, 54, "-", GRAPHITE);
            continue;
        }
        char value[15];
        float position = rack_master_drawn / 100.0f;
        if (master) {
            snprintf(value, sizeof value, "%u%%", rack_master_drawn);
        } else {
            const unsigned param = dial->param;
            const ModuleParamInfo *spec = param < M4_COUNT ? &module_params[param] : &rack_quick_specs[param - M4_COUNT];
            float number = rack_value(param);
            if (dial->format == RACK_FMT_HZ) rack_frequency(param, number, 0, &position);
            else if (param == M4_RESPONSE) position = rack_response_position(number);
            else position = (number - spec->lo) / (spec->hi - spec->lo);
            rack_format(dial, number, value, sizeof value);
        }
        rack_knob(x + 27, y + 21, 11, master ? WHITE : rack_palette[dial->hue], position);
        rack_text_center(x, y + 34, 54, master ? "Master" : dial->label, INK);
        rack_text_center(x, y + 45, 54, value, BLACK);
    }
    zivi_footer_pair("< > / PADS  MODULOS", rack_shows_master() ? "FX  SCOPE" : "PARA+K8  MASTER");
    cv_blit(0, 0);
}

enum { SPLASH_MS = 5000, SPLASH_FRAME_MS = 83 };
static bool splash_active;
static uint32_t splash_started;
static unsigned splash_frame;

static void splash_matrix(unsigned elapsed)
{
    static const struct { uint8_t x, y, bend, end; } paths[] = {
        {7, 7, 55, 73}, {31, 19, 73, 91}, {55, 31, 85, 79}
    };
    static const uint16_t colours[] = {PANEL_CYAN, PANEL_GREEN, AMBER};
    cv_begin(96, 99, BLACK);
    for (int line = 24; line < 96; line += 24) {
        cv_line(line - 2, 0, line - 2, 94, PANEL_GRID);
        cv_line(0, line - 2, 94, line - 2, PANEL_GRID);
    }
    for (int row = 0; row < 16; ++row) {
        for (int col = 0; col < 16; ++col) {
            cv_rect(6 * col, 6 * row, 3, 3, SILVER);
            cv_pset(1 + 6 * col, 1 + 6 * row, BLACK);
        }
    }

    for (unsigned i = 0; i < sizeof paths / sizeof paths[0]; ++i) {
        int x = paths[i].x, y = paths[i].y, bend = paths[i].bend, end = paths[i].end;
        int length = (bend - y) + (end - x);
        int run = clamp(((int)elapsed - (int)i * 720) * length / 1250, 0, length);
        if (!run) continue;
        int dy = clamp(run, 0, bend - y);
        cv_line(x, y, x, y + dy, colours[i]);
        if (run > dy) cv_line(x, bend, x + run - dy, bend, colours[i]);
        cv_rect(x - 2, y - 2, 5, 5, WHITE);
        if (run == length) {
            cv_rect(end - 2, bend - 2, 5, 5, colours[i]);
            int dot = (int)((elapsed / 28u + i * 24u) % (unsigned)length);
            int dy_dot = clamp(dot, 0, bend - y);
            cv_rect(x + dot - dy_dot - 1, y + dy_dot - 1, 3, 3, WHITE);
        }
    }
    cv_blit(73, 26);
}

void app_splash(void)
{
    splash_started = plat_ms();
    splash_frame = 0;
    splash_active = true;
    cv_begin(240, 240, BLACK);
    cv_rect(9, 9, 222, 222, GRAPHITE);
    cv_rect(10, 10, 220, 220, BLACK);
    cv_rect(69, 22, 104, 107, PANEL_GRID);
    cv_rect(70, 23, 102, 105, BLACK);
    cv_rect(20, 20, 30, 2, BLUE);
    cv_rect(190, 20, 30, 2, AMBER);
    cv_rect(16, 134, 208, 58, IVORY);
    cv_rect(16, 134, 208, 2, SILVER);
    cv_rect(16, 189, 208, 3, BLUE);
    cv_text((240 - text_w(&FONT_L, APP_NAME)) / 2, 145, &FONT_L, APP_NAME, INK);
    const char model[] = "SMK-37 Pro / " ZIVI_VERSION;
    cv_text((240 - text_w(&FONT_XS, model)) / 2, 202, &FONT_XS, model, WHITE);
    cv_blit(0, 0);
}

static bool splash_animate(uint32_t now)
{
    if (!splash_active) return false;
    uint32_t elapsed = now - splash_started;
    if (elapsed >= SPLASH_MS) {
        splash_active = false;
        zivi_dirty = 1;
        return false;
    }
    unsigned frame = elapsed / SPLASH_FRAME_MS + 1u;
    if (frame != splash_frame) {
        splash_frame = frame;
        splash_matrix(elapsed);
    }
    return true;
}
