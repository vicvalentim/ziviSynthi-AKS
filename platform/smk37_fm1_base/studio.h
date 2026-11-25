#ifndef ZIVI_FELUCCA_STUDIO_H
#define ZIVI_FELUCCA_STUDIO_H

/* USB MIDI editor, protocol 1. Commands are acknowledged in the main loop. */
enum {
    STUDIO_HELLO = 1, STUDIO_BEGIN, STUDIO_PART, STUDIO_COMMIT,
    STUDIO_CONTROLS, STUDIO_PIN, STUDIO_KS, STUDIO_TRANSPORT, STUDIO_READ, STUDIO_READ_PART, STUDIO_PERFORMANCE,
    STUDIO_STEP, STUDIO_USER, STUDIO_USB_ROUTE, STUDIO_MIDI_CC, STUDIO_PANEL, STUDIO_SYNC, STUDIO_PROFILE,
    STUDIO_DIFF, STUDIO_FIELDS,
    STUDIO_QUICK = 6, STUDIO_PARAMS = STUDIO_QUICK + M4_COUNT,
    STUDIO_KS_BYTES = 10, STUDIO_MATRIX = 256, STUDIO_PART_MAX = 96,
    STUDIO_BASE = STUDIO_PARAMS * 5 + STUDIO_MATRIX + STUDIO_KS_BYTES + 4,
    STUDIO_USB_BYTES = 32, STUDIO_PERF_BYTES = 9, STUDIO_STEP_HEAD = 6, STUDIO_STEP_BYTES = 12,
    STUDIO_GRID = STUDIO_STEP_HEAD + SEQ_STEPS * STUDIO_STEP_BYTES,
    STUDIO_EXT = STUDIO_USB_BYTES + STUDIO_PERF_BYTES + STUDIO_GRID + sizeof midi_cc,
    STUDIO_MAX = STUDIO_BASE + KS_CAPACITY * 5 + STUDIO_EXT,
    STUDIO_OK = 0, STUDIO_INVALID = 1, STUDIO_INCOMPLETE = 2, STUDIO_CHECKSUM = 3
};
static struct {
    uint8_t tag[4], patch[STUDIO_MAX];
} studio_transfer;
#define studio_patch studio_transfer.patch
_Static_assert(sizeof studio_transfer <= PLAT_STORE_MAX, "User preset exceeds sector payload");
static union {
    KsTimedEvent pairs[KS_CAPACITY];
    KsTimedState states[KS_CAPACITY];
} studio_events;
static unsigned studio_expected, studio_received, studio_checksum, studio_read_size;
static uint32_t studio_touched;
static uint32_t studio_session;
static bool studio_xy, studio_grid_transfer;
enum { STUDIO_SYNC_HEAD = 4, STUDIO_DELTA_MAX = STUDIO_PART_MAX - STUDIO_SYNC_HEAD, STUDIO_PIN_DELTA = 64 };
static struct {
    uint32_t revision, base;
    unsigned delta_size;
    float controls[STUDIO_PARAMS];
    uint8_t pins[STUDIO_MATRIX], ks_routes[MATRIX_SIZE], usb_routes[STUDIO_USB_BYTES], cc[sizeof midi_cc];
    uint32_t properties[25];
    KsEvent events[KS_CAPACITY];
    SeqStep steps[SEQ_STEPS];
    uint8_t delta[STUDIO_DELTA_MAX];
    bool full;
} studio_sync;

static unsigned studio_u14(const uint8_t *p)
{
    return p[0] | ((unsigned)p[1] << 7);
}

static unsigned studio_u21(const uint8_t *p)
{
    return studio_u14(p) | ((unsigned)p[2] << 14);
}

static unsigned studio_crc(const uint8_t *bytes, unsigned length)
{
    unsigned crc = 0xffff;
    for (unsigned i = 0; i < length; ++i) {
        crc ^= (unsigned)bytes[i] << 8;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = ((crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0)) & 0xffff;
    }
    return crc;
}

static void studio_septets(uint8_t *p, uint32_t value, unsigned count)
{
    for (unsigned i = 0; i < count; ++i, value >>= 7) p[i] = (uint8_t)(value & 127u);
}

enum {
    PANEL_MASTER, PANEL_SYNTH, PANEL_USB, PANEL_USB_MATRIX, PANEL_RX, PANEL_TX, PANEL_PORT,
    PANEL_EXT, PANEL_CLK_OUT, PANEL_KS_BPM, PANEL_KS_NOTES, PANEL_THRU, PANEL_REL, PANEL_LED, PANEL_COUNT,
    PANEL_RANGE_X = PANEL_COUNT + 2, PANEL_RANGE_Y, PANEL_AKS = PANEL_COUNT + 6
};

static bool studio_panel_set(unsigned key, unsigned value)
{
    if ((key == PANEL_RANGE_X || key == PANEL_RANGE_Y) && value <= 4096) {
        stick_range_set(key - PANEL_RANGE_X, value);
        return true;
    }
    if (key == PANEL_AKS && value <= 1) {
        aks_profile_set(value);
        return true;
    }
    static const uint16_t ceilings[PANEL_COUNT] = {4096, 4096, 4096, 1, 16, 16, 3, 1, 1, 300, 1, 1, 1, PAD_LED_COUNT - 1};
    if (key >= PANEL_COUNT || value > ceilings[key] ||
        (key == PANEL_TX && !value) || (key == PANEL_KS_BPM && value < 30)) return false;
    switch (key) {
    case PANEL_MASTER: plat_master_set(value); break;
    case PANEL_SYNTH: synth_gain = value; break;
    case PANEL_USB: usb_play_gain = value; break;
    case PANEL_USB_MATRIX:
        if (usb_matrix != (bool)value) usb_matrix_set(value);
        if (usb_matrix && !daw_mode) zivi_device.muted = zivi_muted = false;
        break;
    case PANEL_RX:
        if (zivi.midi_channel != value) zivi_smk_midi_disconnect(&zivi_device);
        zivi.midi_channel = value;
        break;
    case PANEL_TX: if (tx_channel + 1 != value) release_outputs(); tx_channel = value - 1; break;
    case PANEL_PORT: if (port_mode != value) release_outputs(); port_mode = value; break;
    case PANEL_EXT: if (midi_clock.external != (bool)value) reset_clock(); midi_clock.external = value; break;
    case PANEL_CLK_OUT: clock_out = value; break;
    case PANEL_KS_BPM: ks_clock_reference = value; break;
    case PANEL_KS_NOTES: ks_notes = value; break;
    case PANEL_THRU:
        usb_thru = value;
        if (!usb_thru) { thru_reset |= thru_channels; thru_channels = 0; }
        break;
    case PANEL_REL: daw_knobs_relative = value; break;
    case PANEL_LED: pad_led_mode = value; break;
    }
    zivi_dirty = 1;
    return true;
}

static float studio_float(const uint8_t *p)
{
    union { uint32_t bits; float value; } encoded = {
        (uint32_t)p[0] | ((uint32_t)p[1] << 7) | ((uint32_t)p[2] << 14) |
        ((uint32_t)p[3] << 21) | ((uint32_t)(p[4] & 15) << 28)
    };
    return encoded.value;
}

static bool studio_parameter(unsigned id, const uint8_t *p)
{
    static const float lo[] = {-24, 0, 0, 0, 0.02f, 5};
    static const float hi[] = {24, 1, 1, 1, 500, 10000};
    if (id >= STUDIO_PARAMS || p[4] > 15) return false;
    float value = studio_float(p);
    if (id < STUDIO_QUICK) return value >= lo[id] && value <= hi[id];
    unsigned param = id - STUDIO_QUICK;
    if (!(value >= module_params[param].lo && value <= module_params[param].hi)) return false;
    return (param != M4_ENV_MODE && param != M4_TRIGGER_MODE) || value == (float)(int)value;
}

static void studio_set(unsigned id, float value)
{
    switch (id) {
    case 0: synth_controls(&zivi, value, zivi.shape, zivi.level); break;
    case 1: synth_controls(&zivi, zivi.tune, value, zivi.level); break;
    case 2: synth_controls(&zivi, zivi.tune, zivi.shape, value); break;
    case 3: synth_patch_controls(&zivi, value, zivi.lfo, zivi.cutoff); break;
    case 4: synth_patch_controls(&zivi, zivi.mix, value, zivi.cutoff); break;
    case 5: synth_patch_controls(&zivi, zivi.mix, zivi.lfo, value); break;
    default: synth_module_control(&zivi, id - STUDIO_QUICK, value); break;
    }
}

static float studio_get(unsigned id)
{
    switch (id) {
    case 0: return zivi.tune;
    case 1: return zivi.shape;
    case 2: return zivi.level;
    case 3: return zivi.mix;
    case 4: return zivi.lfo;
    case 5: return zivi.cutoff;
    default: return zivi.modules.p[id - STUDIO_QUICK];
    }
}

static unsigned studio_axis(float axis)
{
    return 8192 + (int)lround(axis * (axis < 0 ? 8192.0f : 8191.0f));
}

static void studio_panel_read(uint8_t *answer, unsigned fields)
{
    const unsigned values[] = {
        plat_master(), synth_gain, usb_play_gain, usb_matrix, zivi.midi_channel,
        tx_channel + 1, port_mode, midi_clock.external, clock_out, ks_clock_reference,
        ks_notes, usb_thru, daw_knobs_relative, pad_led_mode,
        studio_axis(zivi.stick_x), studio_axis(zivi.stick_y), stick_range[0], stick_range[1],
        studio_axis(stick_position[0]), studio_axis(stick_position[1]), zivi.modules.aks_profile
    };
    _Static_assert(PANEL_AKS + 1 == sizeof values / sizeof *values, "Studio panel fields");
    for (unsigned i = 0; i < fields; ++i) studio_septets(answer + 2 * i, values[i], 2);
}

static bool studio_pin(unsigned row, unsigned col, unsigned type)
{
    return row < 16 && col < 16 && type <= PIN_HALF && synth_pin_supported(row, col);
}

static bool studio_ks_valid(const uint8_t *p)
{
    unsigned realtime = studio_u14(p), spread = studio_u14(p + 2), speed = studio_u14(p + 4);
    return realtime >= 25 && realtime <= 200 && spread >= 25 && spread <= 200 &&
           speed >= 25 && speed <= 400 && p[6] <= 48 && p[7] >= 1 && p[7] <= 30 &&
           p[8] <= 1 && p[9] <= 1;
}

static void studio_ks_apply(const uint8_t *p)
{
    ks_controls(&zivi.ks, studio_u14(p), studio_u14(p + 2), studio_u14(p + 4),
                (int)p[6] - 24, p[7], p[8] != 0, p[9] != 0);
}

static void studio_edited(void)
{
    zivi_device.patch_transition = true;
    zivi_preset = -1;
    zivi_dirty = 1;
}

static bool studio_step_valid(const uint8_t *p)
{
    unsigned span = studio_u21(p + 3), delay = studio_u21(p + 6), duration = studio_u21(p + 9);
    return p[2] <= 1 && span <= ZIVI_SMK_RATE * 30u &&
           (span ? delay < span && duration <= span - delay : !delay && !duration);
}

static void studio_step_apply(SeqStep *step, const uint8_t *p)
{
    *step = (SeqStep){.note = p[0], .velocity = p[1], .legato = p[2],
        .span = studio_u21(p + 3), .delay = studio_u21(p + 6), .duration = studio_u21(p + 9)};
}

static bool studio_grid_valid(const uint8_t *p)
{
    unsigned bpm = studio_u14(p + 1), timing = studio_u14(p + 3);
    return p[0] >= 1 && p[0] <= SEQ_STEPS && bpm >= 30 && bpm <= 300 &&
           timing >= 30 && timing <= 300 && p[5] < ARP_DIVISION_COUNT;
}

static bool studio_steps_valid(const uint8_t *p)
{
    if (!studio_grid_valid(p)) return false;
    p += STUDIO_STEP_HEAD;
    for (unsigned i = 0; i < SEQ_STEPS; ++i, p += STUDIO_STEP_BYTES)
        if (!studio_step_valid(p)) return false;
    return true;
}

static void studio_steps_apply(const uint8_t *p)
{
    StepSequencer *seq = &zivi.sequencer;
    synth_seq_play(&zivi, false);
    seq->division = (enum ArpDivision)p[5];
    seq->timing_bpm = studio_u14(p + 3);
    const uint8_t *step = p + STUDIO_STEP_HEAD;
    for (unsigned i = 0; i < SEQ_STEPS; ++i, step += STUDIO_STEP_BYTES)
        studio_step_apply(&seq->steps[i], step);
    synth_seq_timing(&zivi, p[0], studio_u14(p + 1));
    seq_entry = seq_follow = false;
    seq_page = seq_cursor = 0;
}

static bool studio_extension_valid(const uint8_t *p)
{
    for (unsigned i = 0; i < STUDIO_USB_BYTES; ++i)
        if (p[i] > PIN_HALF || (p[i] && !synth_pin_supported(SRC_INPUT1, i % 16))) return false;
    p += STUDIO_USB_BYTES;
    unsigned bpm = studio_u14(p + 1);
    if (p[0] > 7 || bpm < 30 || bpm > 300 || p[3] < 10 || p[3] > 95 ||
        p[4] >= ARP_ORDER_COUNT || p[5] >= ARP_DIVISION_COUNT || p[6] > 100 || p[7] >= 12 || p[8] >= SCALE_COUNT)
        return false;
    p += STUDIO_PERF_BYTES;
    return studio_steps_valid(p);
}

static void studio_extension_apply(const uint8_t *p)
{
    bool usb = false;
    for (unsigned i = 0; i < STUDIO_USB_BYTES; ++i)
        if (p[i]) {
            synth_usb_route(&zivi, i / 16, i % 16, (enum Pin)p[i]);
            usb = true;
        }
    if (usb && !usb_matrix) usb_matrix_set(true);
    p += STUDIO_USB_BYTES;
    synth_arp(&zivi, p[0] & 1u, studio_u14(p + 1), p[3], (enum ArpOrder)p[4], (enum ArpDivision)p[5]);
    synth_arp_modes(&zivi, p[0] & 2u, p[0] & 4u, p[6]);
    set_scale(p[7], (enum ScaleMode)p[8]);
    p += STUDIO_PERF_BYTES;
    studio_steps_apply(p);
    memcpy(midi_cc, p + STUDIO_GRID, sizeof midi_cc);
}

static bool studio_apply(void)
{
    const uint8_t *pins = studio_patch + STUDIO_PARAMS * 5;
    const uint8_t *ks = pins + STUDIO_MATRIX;
    const uint8_t *events = ks + STUDIO_KS_BYTES + 4;
    unsigned duration = studio_u21(ks + STUDIO_KS_BYTES);
    unsigned flags = ks[STUDIO_KS_BYTES + 3];
    unsigned ext = flags & 8u ? STUDIO_EXT : 0;
    if (studio_received < STUDIO_BASE + ext) return false;
    unsigned count = (studio_received - STUDIO_BASE - ext) / 5;
    const uint8_t *extension = events + count * 5;
    bool xy = (flags & 2u) != 0, ems = (flags & 4u) != 0;
    bool format = (flags & 1u) != 0;
    if (flags > 15 || ((ems || ext) && !xy) || !studio_ks_valid(ks) || count > KS_CAPACITY ||
        studio_received != STUDIO_BASE + count * 5 + ext || (count == 0 && duration != 0) ||
        (ext && !studio_extension_valid(extension))) return false;
    for (unsigned i = 0; i < STUDIO_PARAMS; ++i)
        if (!studio_parameter(i, studio_patch + i * 5)) return false;
    for (unsigned i = 0; i < STUDIO_MATRIX; ++i) {
        unsigned type = pins[i];
        if (xy && i / MATRIX_SIZE == SRC_STICK_Y) {
            if (type > 8) return false;
            type %= 3;
        }
        if (!studio_pin(i / MATRIX_SIZE, i % MATRIX_SIZE, type)) return false;
    }
    for (unsigned i = 0; i < count; ++i) {
        const uint8_t *p = events + i * 5;
        bool attack = format && (p[2] & 64u);
        unsigned ms = studio_u14(p) | ((unsigned)(p[2] & (format ? 63u : 127u)) << 14);
        if (ms >= duration || ms > 30000) return false;
        if (format) studio_events.states[i] = (KsTimedState){ (uint16_t)ms, p[3], p[4], attack };
        else studio_events.pairs[i] = (KsTimedEvent){ (uint16_t)ms, p[3], p[4] };
    }
    if (count && !(format ? ks_load_states(&zivi.ks, studio_events.states, count, duration)
                          : ks_load_ms(&zivi.ks, studio_events.pairs, count, duration))) return false;
    if (!count) {
        ks_stop(&zivi.ks);
        zivi.ks.count = zivi.ks.length = 0;
        zivi.ks.limit = KS_LIMIT_NONE;
    }
    studio_ks_apply(ks);
    factory_phrase = count != 0;
    if (count) import_ks();
    synth_clear_matrix(&zivi);
    for (unsigned i = 0; i < STUDIO_PARAMS; ++i) studio_set(i, studio_float(studio_patch + i * 5));
    for (unsigned i = 0; i < STUDIO_MATRIX - MATRIX_SIZE; ++i)
        if (pins[i]) synth_pin(&zivi, i / MATRIX_SIZE, i % MATRIX_SIZE, (enum Pin)pins[i]);
    synth_ks_matrix(&zivi, ems);
    for (unsigned col = 0; col < MATRIX_SIZE; ++col) {
        unsigned type = pins[STUDIO_MATRIX - MATRIX_SIZE + col];
        synth_ks_route(&zivi, col, (enum Pin)(xy ? type / 3 : type));
        if (xy) synth_pin(&zivi, SRC_STICK_Y, col, (enum Pin)(type % 3));
    }
    if (ext) studio_extension_apply(extension);
    studio_xy = xy;
    zivi_device.muted = zivi_muted = daw_mode;
    selected_preset = NO_NOTE;
    studio_edited();
    return true;
}

static void studio_read_ks(uint8_t *p)
{
    const Ks *ks = &zivi.ks;
    studio_septets(p, ks->realtime_spread, 2);
    studio_septets(p + 2, ks->sequence_spread, 2);
    studio_septets(p + 4, ks->speed, 2);
    p[6] = (uint8_t)(ks->transpose + 24);
    p[7] = (uint8_t)ks->limit_seconds;
    p[8] = ks->loop;
    p[9] = ks->sequence_trigger;
}

static unsigned studio_read_base(bool xy, bool *sequence_omitted)
{
    for (unsigned i = 0; i < STUDIO_PARAMS; ++i)
        studio_septets(studio_patch + i * 5, label_bits(studio_get(i)), 5);
    uint8_t *pins = studio_patch + STUDIO_PARAMS * 5;
    memcpy(pins, zivi.matrix, STUDIO_MATRIX);
    for (unsigned col = 0; col < MATRIX_SIZE; ++col) {
        uint8_t *slot = &pins[SRC_STICK_Y * MATRIX_SIZE + col];
        if (xy) *slot += 3u * zivi.ks_routes[col];
        else if (!zivi.ks_matrix) *slot = zivi.ks_routes[col];
    }
    uint8_t *p = pins + STUDIO_MATRIX;
    const Ks *ks = &zivi.ks;
    studio_read_ks(p);
    p += STUDIO_KS_BYTES;
    memset(p, 0, 3);
    p[3] = xy ? (zivi.ks_matrix ? 6 : 2) : 0;
    unsigned length = (ks->length * 1000u + ks->rate / 2u) / ks->rate;
    *sequence_omitted = ks->count != 0;
    if (ks->mode == KS_RECORDING || !ks->count) return STUDIO_BASE;
    if (!length) length = 1;
    if (length > 30000) length = 30000;
    unsigned count = 0;
    for (unsigned i = 0; i < ks->count; ++i) {
        const KsEvent *event = &ks->events[i];
        unsigned ms = (event->frame * 1000u + ks->rate / 2u) / ks->rate;
        if (ms >= length) ms = length - 1;
        KsTimedState state = { (uint16_t)ms, event->note, event->velocity, event->attack };
        if (count && studio_events.states[count - 1].ms == ms) {
            state.attack |= studio_events.states[--count].attack && state.velocity != 0;
        }
        if (count || state.velocity) studio_events.states[count++] = state;
    }
    if (!count || !studio_events.states[0].attack) return STUDIO_BASE;
    bool pairs = count % 2 == 0;
    for (unsigned i = 0; pairs && i < count; i += 2) {
        const KsTimedState *on = &studio_events.states[i], *off = &studio_events.states[i + 1];
        pairs = on->velocity && on->attack && !off->velocity && on->note == off->note;
    }
    studio_septets(p, length, 3);
    if (!pairs) p[3] |= 1u;
    uint8_t *wire = p + 4;
    for (unsigned i = 0; i < count; ++i, wire += 5) {
        const KsTimedState *event = &studio_events.states[i];
        studio_septets(wire, event->ms, 3);
        if (!pairs && event->attack) wire[2] |= 64;
        wire[3] = event->note;
        wire[4] = event->velocity;
    }
    *sequence_omitted = false;
    return STUDIO_BASE + count * 5;
}

static void studio_read_steps(uint8_t *p)
{
    const StepSequencer *seq = &zivi.sequencer;
    p[0] = seq->length;
    studio_septets(p + 1, seq->bpm, 2);
    studio_septets(p + 3, seq->timing_bpm ? seq->timing_bpm : seq->bpm, 2);
    p[5] = seq->division;
    p += STUDIO_STEP_HEAD;
    for (unsigned i = 0; i < SEQ_STEPS; ++i, p += STUDIO_STEP_BYTES) {
        const SeqStep *step = &seq->steps[i];
        p[0] = step->note; p[1] = step->velocity; p[2] = step->legato;
        studio_septets(p + 3, step->span, 3);
        studio_septets(p + 6, step->delay, 3);
        studio_septets(p + 9, step->duration, 3);
    }
}

static unsigned studio_read_patch(bool xy, bool extended, bool *omitted)
{
    unsigned length = studio_read_base(xy, omitted);
    if (!extended) return length;
    studio_patch[STUDIO_BASE - 1] |= 8u;
    uint8_t *p = studio_patch + length;
    memcpy(p, zivi.usb_routes, STUDIO_USB_BYTES);
    p += STUDIO_USB_BYTES;
    const Arp *arp = &zivi.arp;
    p[0] = (uint8_t)(arp->enabled | arp->hold << 1 | arp->free_run << 2);
    studio_septets(p + 1, arp->bpm, 2);
    p[3] = arp->gate; p[4] = arp->order; p[5] = arp->division;
    p[6] = arp->chance; p[7] = scale_root; p[8] = scale_mode;
    p += STUDIO_PERF_BYTES;
    studio_read_steps(p);
    p += STUDIO_GRID;
    memcpy(p, midi_cc, sizeof midi_cc);
    return length + STUDIO_EXT;
}

/* Compare native state only. A steady poll never encodes a patch or calculates its CRC. */
static bool studio_observe(void *previous, const void *current, size_t size)
{
    if (memcmp(previous, current, size) == 0) return false;
    memcpy(previous, current, size);
    return true;
}

static uint8_t *studio_delta_append(unsigned size, unsigned *used, bool *full)
{
    if (*used + size > STUDIO_DELTA_MAX) *full = true;
    if (*full) return NULL;
    uint8_t *out = studio_sync.delta + *used;
    *used += size;
    return out;
}

static void studio_sync_observe(void)
{
    const Ks *ks = &zivi.ks;
    const StepSequencer *seq = &zivi.sequencer;
    const Arp *arp = &zivi.arp;
    const uint32_t settings[] = {
        ks->length, ks->count, ks->realtime_spread, ks->sequence_spread, ks->speed, ks->limit_seconds,
        (uint32_t)ks->transpose, ks->loop, ks->sequence_trigger, zivi.ks_matrix,
        seq->length, seq->bpm, seq->timing_bpm, seq->division,
        arp->enabled, arp->hold, arp->free_run, arp->bpm, arp->gate, arp->order, arp->division,
        arp->chance, scale_root, scale_mode, (uint32_t)zivi_preset
    };
    _Static_assert(sizeof settings == sizeof studio_sync.properties, "Studio properties");
    /* A knob edit clears the preset index. A new named preset needs a full snapshot. */
    bool full = zivi_preset >= 0 && studio_sync.properties[24] != (uint32_t)zivi_preset;
    studio_sync.properties[24] = (uint32_t)zivi_preset;
    full |= studio_observe(studio_sync.properties, settings, sizeof settings);
    full |= studio_observe(studio_sync.ks_routes, zivi.ks_routes, sizeof studio_sync.ks_routes);
    full |= studio_observe(studio_sync.usb_routes, zivi.usb_routes, sizeof studio_sync.usb_routes);
    full |= studio_observe(studio_sync.cc, midi_cc, sizeof studio_sync.cc);
    full |= studio_observe(studio_sync.events, ks->events, ks->count * sizeof *ks->events);
    full |= studio_observe(studio_sync.steps, seq->steps, sizeof studio_sync.steps);

    unsigned used = 0;
    for (unsigned i = 0; i < STUDIO_PARAMS; ++i) {
        float value = studio_get(i);
        if (studio_sync.controls[i] == value) continue;
        studio_sync.controls[i] = value;
        uint8_t *out = studio_delta_append(6, &used, &full);
        if (out) {
            out[0] = i;
            studio_septets(out + 1, label_bits(value), 5);
        }
    }
    const uint8_t *pins = (const uint8_t *)&zivi.matrix;
    if (memcmp(studio_sync.pins, pins, STUDIO_MATRIX)) {
        for (unsigned i = 0; i < STUDIO_MATRIX; ++i) {
            if (studio_sync.pins[i] == pins[i]) continue;
            studio_sync.pins[i] = pins[i];
            uint8_t *out = studio_delta_append(4, &used, &full);
            if (out) {
                out[0] = STUDIO_PIN_DELTA;
                out[1] = i >> 4;
                out[2] = i & 15;
                out[3] = pins[i];
            }
        }
    }
    if (full || used || !studio_sync.revision) {
        studio_sync.base = studio_sync.revision;
        studio_sync.revision = (studio_sync.revision % 0x1fffffu) + 1u;
        studio_sync.delta_size = used;
        studio_sync.full = full;
    }
}

static void studio_follow(uint32_t now, unsigned known, uint8_t *answer, unsigned *size)
{
    if (studio_expected || zivi.ks.mode == KS_RECORDING || zivi.sequencer.recording) {
        answer[0] = 0;
        studio_septets(answer + 1, known, 3);
        *size = STUDIO_SYNC_HEAD;
        return;
    }
    studio_sync_observe();
    studio_septets(answer + 1, studio_sync.revision, 3);
    *size = STUDIO_SYNC_HEAD;
    if (known == studio_sync.revision) {
        answer[0] = 0;
    } else if (known && known == studio_sync.base && !studio_sync.full) {
        answer[0] = 1;
        memcpy(answer + STUDIO_SYNC_HEAD, studio_sync.delta, studio_sync.delta_size);
        *size += studio_sync.delta_size;
    } else {
        answer[0] = 2;
        bool omitted;
        studio_xy = true;
        studio_read_size = studio_read_patch(true, true, &omitted);
        studio_touched = now;
        studio_septets(answer + 4, studio_read_size, 2);
        studio_septets(answer + 6, studio_crc(studio_patch, studio_read_size), 3);
        answer[9] = omitted;
        *size = 10;
    }
}

#include "studio_diff.h"

static bool user_slot_present(unsigned slot)
{
    uint8_t tag[4];
    return slot < APP_USER_SLOTS && plat_store_head(OBJ_USER_1 + slot, tag, sizeof tag) == sizeof tag &&
           memcmp(tag, APP_USER_TAG, sizeof tag) == 0;
}

static unsigned user_slot_operation(unsigned slot, bool save, bool grid_only)
{
    if (slot >= APP_USER_SLOTS || daw_mode || zivi.ks.mode == KS_RECORDING || zivi.sequencer.recording)
        return STUDIO_INVALID;
    if ((studio_expected || studio_read_size) && studio_session == plat_usb_session() &&
        plat_ms() - studio_touched <= 2000u) return STUDIO_BUSY;
    studio_expected = studio_read_size = 0;
    unsigned obj = OBJ_USER_1 + slot;
    if (save && (zivi.ks.mode == KS_PLAYING || zivi.sequencer.playing || zivi.held_count ||
        (zivi.arp.enabled && (zivi.arp.free_run || zivi.arp.latched_count)) || web_attack || panel_attack))
        return STUDIO_BUSY;
    if (save && !grid_only) {
        bool omitted;
        unsigned length = studio_read_patch(true, true, &omitted);
        if (omitted) return STUDIO_INVALID;
        memcpy(studio_transfer.tag, APP_USER_TAG, sizeof studio_transfer.tag);
        return plat_store_save(obj, &studio_transfer, length + sizeof studio_transfer.tag) ?
               STUDIO_STORAGE : STUDIO_OK;
    }
    int length = plat_store_size(obj);
    if (length < (int)(sizeof studio_transfer.tag + STUDIO_BASE + STUDIO_EXT) ||
        length > (int)sizeof studio_transfer ||
        plat_store_load(obj, &studio_transfer, (unsigned)length) != length ||
        memcmp(studio_transfer.tag, APP_USER_TAG, sizeof studio_transfer.tag)) return STUDIO_STORAGE;
    studio_received = (unsigned)length - sizeof studio_transfer.tag;
    if (!grid_only) return studio_apply() ? STUDIO_OK : STUDIO_INVALID;
    uint8_t *extension = studio_patch + studio_received - STUDIO_EXT;
    if (!(studio_patch[STUDIO_BASE - 1] & 8u) || !studio_extension_valid(extension)) return STUDIO_INVALID;
    studio_read_steps(extension + STUDIO_USB_BYTES + STUDIO_PERF_BYTES);
    return plat_store_save(obj, &studio_transfer, (unsigned)length) ? STUDIO_STORAGE : STUDIO_OK;
}

static unsigned studio_command(unsigned command, const uint8_t *p, unsigned n,
                               uint8_t *answer, unsigned *answer_size)
{
    uint32_t now = plat_ms();
    uint32_t session = plat_usb_session();
    if (studio_session != session || now - studio_touched > 2000u)
        studio_expected = studio_read_size = 0;
    if (studio_session != session) studio_diff_served = 0;
    studio_session = session;
    switch (command) {
    case STUDIO_HELLO:
        if (n) return STUDIO_INVALID;
        studio_expected = studio_read_size = 0;
        answer[0] = 1; /* SMK-37 Pro */
        answer[1] = 98;
        answer[2] = 12;
        studio_diff_served = 0;
        studio_xy = false;
        answer[3] = STUDIO_PART_MAX;
        *answer_size = 4;
        return STUDIO_OK;
    case STUDIO_BEGIN: {
        studio_expected = studio_read_size = 0;
        if (n != 5 && (n != 6 || p[5] != 1)) return STUDIO_INVALID;
        unsigned length = studio_u14(p);
        studio_grid_transfer = n == 6;
        if (p[4] > 3 || (studio_grid_transfer ? length != STUDIO_GRID : length < STUDIO_BASE || length > STUDIO_MAX))
            return STUDIO_INVALID;
        studio_expected = length;
        studio_checksum = studio_u21(p + 2);
        studio_received = 0;
        studio_touched = now;
        return STUDIO_OK;
    }
    case STUDIO_PART:
        if (n < 3 || n > 2 + STUDIO_PART_MAX) return STUDIO_INVALID;
        if (!studio_expected || studio_received != studio_u14(p) || studio_received + n - 2 > studio_expected)
            return STUDIO_INCOMPLETE;
        memcpy(studio_patch + studio_received, p + 2, n - 2);
        studio_received += n - 2;
        studio_touched = now;
        return STUDIO_OK;
    case STUDIO_COMMIT: {
        if (n || !studio_expected || studio_received != studio_expected) return STUDIO_INCOMPLETE;
        studio_expected = 0;
        if (studio_crc(studio_patch, studio_received) != studio_checksum) return STUDIO_CHECKSUM;
        if (!studio_grid_transfer) return studio_apply() ? STUDIO_OK : STUDIO_INVALID;
        if (daw_mode || !studio_steps_valid(studio_patch)) return STUDIO_INVALID;
        if (zivi.sequencer.playing || zivi.sequencer.recording || zivi.ks.mode != KS_STOPPED) return STUDIO_BUSY;
        studio_steps_apply(studio_patch);
        zivi_dirty = 1;
        return STUDIO_OK;
    }
    case STUDIO_CONTROLS:
        if (!n || n % 6) return STUDIO_INVALID;
        for (unsigned i = 0; i < n; i += 6)
            if (!studio_parameter(p[i], p + i + 1)) return STUDIO_INVALID;
        for (unsigned i = 0; i < n; i += 6) studio_set(p[i], studio_float(p + i + 1));
        studio_edited();
        return STUDIO_OK;
    case STUDIO_PIN:
        if (!n || n % 3) return STUDIO_INVALID;
        for (unsigned i = 0; i < n; i += 3)
            if (!studio_pin(p[i], p[i + 1], p[i + 2])) return STUDIO_INVALID;
        for (unsigned i = 0; i < n; i += 3) {
            if (p[i] == SRC_STICK_Y && !studio_xy && !zivi.ks_matrix)
                synth_ks_route(&zivi, p[i + 1], (enum Pin)p[i + 2]);
            else synth_pin(&zivi, p[i], p[i + 1], (enum Pin)p[i + 2]);
        }
        studio_edited();
        return STUDIO_OK;
    case STUDIO_KS:
        if (n != STUDIO_KS_BYTES || !studio_ks_valid(p)) return STUDIO_INVALID;
        studio_ks_apply(p);
        zivi_dirty = 1;
        return STUDIO_OK;
    case STUDIO_TRANSPORT:
        if (n != 1 || p[0] > 2) return STUDIO_INVALID;
        if (p[0] <= 1 && zivi.sequencer.playing) synth_seq_play(&zivi, false);
        if (p[0] == 0) {
            bool recorded = zivi.ks.mode == KS_RECORDING;
            ks_stop(&zivi.ks);
            if (recorded) import_ks();
        }
        if (p[0] == 1 && !ks_play(&zivi.ks)) return STUDIO_INVALID;
        if (p[0] == 2) {
            synth_ks_record(&zivi);
            factory_phrase = false;
        }
        zivi_dirty = 1;
        return STUDIO_OK;
    case STUDIO_READ: {
        bool compare = n == 6 && p[0] == 3 && p[5] <= 3;
        if (!compare && n && (n != 1 || p[0] < 1 || p[0] > 2)) return STUDIO_INVALID;
        if (compare && (studio_expected || zivi.ks.mode == KS_RECORDING || zivi.sequencer.recording)) {
            memset(answer, 0, 6);
            *answer_size = 6;
            return STUDIO_OK;
        }
        bool omitted;
        studio_xy = n == 1;
        if (compare) studio_xy = true;
        studio_expected = 0;
        studio_read_size = studio_read_patch(studio_xy, compare || (n && p[0] == 2), &omitted);
        unsigned checksum = studio_crc(studio_patch, studio_read_size);
        if (compare && studio_read_size == studio_u14(p + 1) && checksum == studio_u21(p + 3))
            studio_read_size = 0;
        studio_touched = now;
        studio_septets(answer, studio_read_size, 2);
        studio_septets(answer + 2, checksum, 3);
        answer[5] = omitted;
        *answer_size = 6;
        return STUDIO_OK;
    }
    case STUDIO_READ_PART: {
        if (n != 2) return STUDIO_INVALID;
        unsigned offset = studio_u14(p);
        if (offset >= studio_read_size) return STUDIO_INCOMPLETE;
        unsigned length = studio_read_size - offset;
        if (length > STUDIO_PART_MAX) length = STUDIO_PART_MAX;
        memcpy(answer, studio_patch + offset, length);
        *answer_size = length;
        if (offset + length == studio_read_size) studio_read_size = 0;
        studio_touched = now;
        return STUDIO_OK;
    }
    case STUDIO_USER:
        if (n == 1 && p[0] == 0) {
            for (unsigned i = 0; i < APP_USER_SLOTS; ++i) answer[i] = user_slot_present(i);
            *answer_size = APP_USER_SLOTS;
            return STUDIO_OK;
        }
        if (n == 2 && p[0] >= 1 && p[0] <= 3) return user_slot_operation(p[1], p[0] != 1, p[0] == 3);
        return STUDIO_INVALID;
    case STUDIO_MIDI_CC:
        if (n != 2 || p[0] >= sizeof midi_cc || p[1] > 127) return STUDIO_INVALID;
        midi_cc[p[0]] = p[1];
        zivi_dirty = 1;
        return STUDIO_OK;
    case STUDIO_SYNC:
        if (n != 3) return STUDIO_INVALID;
        studio_follow(now, studio_u21(p), answer, answer_size);
        return STUDIO_OK;
    case STUDIO_DIFF:
        return studio_diff_command(p, n, answer, answer_size);
    case STUDIO_FIELDS:
        return studio_fields(p, n) ? STUDIO_OK : STUDIO_INVALID;
    case STUDIO_PROFILE: {
        if (n > 1 || (n && p[0] != 1)) return STUDIO_INVALID;
        if (n) profile_reset();
        const uint32_t values[] = {profile_blocks, peak_ticks / 24u, profile_over_budget,
            profile_min_ready == ZIVI_SMK_SLOTS ? UINT32_MAX : profile_min_ready,
            zivi_device.underruns - profile_start_miss, plat_xruns() - profile_start_dma,
            usb_input_dropped - profile_start_usb, profile_studio_peak / 24u};
        for (unsigned i = 0; i < sizeof values / sizeof *values; ++i) studio_septets(answer + 5 * i, values[i], 5);
        *answer_size = sizeof values / sizeof *values * 5;
        return STUDIO_OK;
    }
    case STUDIO_PANEL:
        if (!n || (n == 1 && p[0] == 1)) {
            unsigned fields = PANEL_COUNT + (n ? 6 : 2);
            studio_panel_read(answer, fields);
            *answer_size = 2 * fields;
            return STUDIO_OK;
        }
        return n == 3 && studio_panel_set(p[0], studio_u14(p + 1)) ? STUDIO_OK : STUDIO_INVALID;
    case STUDIO_USB_ROUTE:
        if (n != 3 || !synth_usb_route(&zivi, p[0], p[1], (enum Pin)p[2])) return STUDIO_INVALID;
        if (p[2] && !usb_matrix) usb_matrix_set(true);
        studio_edited();
        return STUDIO_OK;
    case STUDIO_STEP:
        if (!n || daw_mode) return STUDIO_INVALID;
        if (p[0] == 4 && n == 1) {
            if (zivi.sequencer.recording) return STUDIO_BUSY;
            studio_expected = 0;
            studio_read_steps(studio_patch);
            studio_read_size = STUDIO_GRID;
            studio_touched = now;
            studio_septets(answer, STUDIO_GRID, 2);
            studio_septets(answer + 2, studio_crc(studio_patch, STUDIO_GRID), 3);
            *answer_size = 5;
            return STUDIO_OK;
        }
        if (p[0] == 2 && n == 2 && p[1] <= 1) {
            synth_seq_play(&zivi, p[1]);
            if (p[1]) zivi_device.muted = zivi_muted = false;
        } else {
            if (zivi.sequencer.recording) return STUDIO_BUSY;
            if (p[0] == 0 && (n == 4 || n == 5)) {
                unsigned bpm = studio_u14(p + 2);
                unsigned division = n == 5 ? p[4] : zivi.sequencer.division;
                if (!p[1] || p[1] > SEQ_STEPS || bpm < 30 || bpm > 300 || division >= ARP_DIVISION_COUNT)
                    return STUDIO_INVALID;
                zivi.sequencer.division = (enum ArpDivision)division;
                synth_seq_timing(&zivi, p[1], bpm);
            } else if (p[0] == 1 && n == 4) {
                if (!synth_seq_set(&zivi, p[1], p[2], p[3])) return STUDIO_INVALID;
            } else if (p[0] == 3 && n == 1) {
                seq_toggle_record();
            } else return STUDIO_INVALID;
        }
        zivi_dirty = 1;
        return STUDIO_OK;
    case STUDIO_PERFORMANCE:
        if (n == 4 && p[0] == 11 && p[1] < 2) {
            int value = (int)studio_u14(p + 2) - 8192;
            stick_set(p[1], value / (value < 0 ? 8192.0f : 8191.0f));
            return STUDIO_OK;
        }
        if (n == 2 && p[0] == 0 && p[1] <= 1) {
            web_attack = p[1] != 0;
            web_attack_ms = now;
            return STUDIO_OK;
        }
        if (n == 3 && (p[0] == 1 || p[0] == 8)) {
            unsigned value = studio_u14(p + 1);
            float axis = ((int)value - 8192) / (value >= 8192 ? 8191.0f : 8192.0f);
            const unsigned index = p[0] == 1 ? 0 : 1;
            stick_set(index, stick_range[index] ? axis * 4096.0f / stick_range[index] : 0.0f);
            return STUDIO_OK;
        }
        if (n == 3 && p[0] == 9 && synth_ks_route(&zivi, p[1], (enum Pin)p[2])) {
            studio_edited();
            return STUDIO_OK;
        }
        if (n == 2 && p[0] == 10 && p[1] <= 1) {
            synth_ks_matrix(&zivi, p[1] != 0);
            studio_edited();
            return STUDIO_OK;
        }
        if ((n == 6 || n == 7) && p[0] == 2 && p[1] <= 1 &&
            p[4] >= 10 && p[4] <= 95 && p[5] < ARP_ORDER_COUNT && !zivi.sequencer.recording) {
            unsigned bpm = studio_u14(p + 2);
            unsigned division = n == 7 ? p[6] : (unsigned)zivi.arp.division;
            if (bpm < 30 || bpm > 300 || division >= ARP_DIVISION_COUNT) return STUDIO_INVALID;
            synth_arp(&zivi, p[1], bpm, p[4], (enum ArpOrder)p[5], (enum ArpDivision)division);
            if (zivi.arp.enabled && zivi.arp.free_run && !daw_mode)
                zivi_device.muted = zivi_muted = false;
            zivi_dirty = 1;
            return STUDIO_OK;
        }
        if (n == 3 && p[0] == 3 && p[1] < 12 && p[2] < SCALE_COUNT) {
            set_scale(p[1], (enum ScaleMode)p[2]);
            return STUDIO_OK;
        }
        if ((n == 1 && (p[0] == 4 || p[0] == 6 || p[0] == 7)) || (n == 4 && p[0] == 7)) {
            const Ks *ks = &zivi.ks;
            bool recording = ks->mode == KS_RECORDING;
            unsigned at = recording ? ks->record_frame : ks->position / 100u;
            unsigned end = recording ? ks->record_limit : ks->length;
            answer[0] = ks->mode;
            studio_septets(answer + 1, ks->count, 2);
            answer[3] = ks->limit;
            studio_septets(answer + 4, (at * 1000u) / ks->rate, 3);
            studio_septets(answer + 7, (end * 1000u) / ks->rate, 3);
            answer[10] = (uint8_t)(zivi.arp.enabled | (zivi.arp.velocity ? 2u : 0u) | ((unsigned)zivi.arp.division << 2));
            studio_septets(answer + 11, zivi.arp.bpm, 2);
            answer[13] = zivi.arp.gate;
            answer[14] = zivi.arp.order;
            answer[15] = (uint8_t)scale_root;
            answer[16] = (uint8_t)scale_mode;
            answer[17] = zivi.arp.velocity ? (uint8_t)zivi.arp.note : 0;
            answer[18] = (uint8_t)(zivi.arp.hold | (zivi.arp.free_run << 1) |
                                    (zivi.sequencer.recording << 2) | (zivi.sequencer.playing << 3));
            answer[19] = (uint8_t)zivi.arp.chance;
            *answer_size = p[0] == 4 ? 18 : 20;
            if (p[0] == 7) {
                const StepSequencer *seq = &zivi.sequencer;
                answer[20] = seq->index;
                answer[21] = (uint8_t)(midi_clock.external | (midi_clock.locked << 1));
                studio_septets(answer + 22, midi_clock.external && midi_clock.locked ? sync_bpm() : seq->bpm, 2);
                *answer_size = 24;
            }
            if (n == 4 && studio_crc(answer, *answer_size) == studio_u21(p + 1)) *answer_size = 0;
            return STUDIO_OK;
        }
        if (n == 3 && p[0] == 5 && p[1] <= 3 && p[2] <= 100) {
            synth_arp_modes(&zivi, p[1] & 1u, p[1] & 2u, p[2]);
            if (zivi.arp.enabled && zivi.arp.free_run && !daw_mode)
                zivi_device.muted = zivi_muted = false;
            zivi_dirty = 1;
            return STUDIO_OK;
        }
        return STUDIO_INVALID;
    default:
        return STUDIO_INVALID;
    }
}

int app_sysex(const uint8_t *data, uint32_t size)
{
    static const uint8_t signature[] = {0x7d, 'Z', 'S', 1};
    if (size < 7 || memcmp(data, signature, sizeof signature)) return 0;
    for (unsigned i = 0; i < size; ++i) if (data[i] >= 128) return 1;
    const uint8_t command = data[4];
    if (command >= 64) return 1;
    unsigned answer_size = 0;
    uint8_t reply[10 + STUDIO_PART_MAX] = {
        0xf0, 0x7d, 'Z', 'S', 1, (uint8_t)(command | 64), data[5], data[6]
    };
    uint32_t start = plat_ticks24();
    reply[8] = (uint8_t)studio_command(command, data + 7, size - 7, reply + 9, &answer_size);
    uint32_t elapsed = plat_ticks24() - start;
    if (command != STUDIO_PROFILE && elapsed > profile_studio_peak) profile_studio_peak = elapsed;
    reply[9 + answer_size] = 0xf7;
    (void)plat_sysex_send(reply, 10 + answer_size);
    return 1;
}

#endif
