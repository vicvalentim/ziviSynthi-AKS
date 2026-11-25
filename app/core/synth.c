#include "synth.h"

#include "dsp_curve.h"
#include "aks_calibration.h"
#include <string.h>

static ZiviReal clamp(ZiviReal value, ZiviReal minimum, ZiviReal maximum)
{
    return dsp_min(maximum, dsp_max(minimum, value));
}

typedef struct {
    const char *label;
    int stage;
} Endpoint;

static const Endpoint sources[MATRIX_SIZE] = {
#define SOURCE(id, index, label, stage) [index] = {label, stage},
#define DEST(id, index, label, stage)
#include "matrix.def"
#undef SOURCE
#undef DEST
};
static const Endpoint destinations[MATRIX_SIZE] = {
#define SOURCE(id, index, label, stage)
#define DEST(id, index, label, stage) [index] = {label, stage},
#include "matrix.def"
#undef SOURCE
#undef DEST
};

const char *synth_source_name(unsigned row)
{
    return row < MATRIX_SIZE ? sources[row].label : "";
}

const char *synth_dest_name(unsigned col)
{
    return col < MATRIX_SIZE ? destinations[col].label : "";
}

bool synth_pin_supported(unsigned row, unsigned col)
{
    return row < MATRIX_SIZE && col < MATRIX_SIZE
        && sources[row].stage >= 0 && destinations[col].stage >= 0;
}

bool synth_pin_delayed(unsigned row, unsigned col)
{
    return synth_pin_supported(row, col) && sources[row].stage >= destinations[col].stage;
}

static void rebuild_column(Synth *synth, unsigned col)
{
    unsigned count = 0;
    for (unsigned row = 0; row < MATRIX_SIZE; ++row) {
        uint8_t pin = synth->matrix[row][col];
        if (pin == PIN_OFF) continue;
        bool delayed = synth_pin_delayed(row, col);
        ZiviReal gain = pin == PIN_FULL ? 1.0 : 0.5;
        synth->routes[col][count++] = (Route){(uint8_t)row, delayed, delayed ? gain * 0.72 : gain};
    }
    uint8_t ks = synth->ks_matrix ? PIN_OFF : synth->ks_routes[col];
    if (ks) synth->routes[col][count++] = (Route){SRC_KS_INTERNAL, false, ks == PIN_FULL ? 1.0 : 0.5};
    for (unsigned channel = 0; channel < 2; ++channel) {
        unsigned amount = synth->usb_routes[channel][col];
        if (amount) synth->routes[col][count++] = (Route){(uint8_t)(SRC_USB_L + channel), false,
                                                        amount == PIN_FULL ? 1.0 : 0.5};
    }
    synth->counts[col] = (uint8_t)count;
}

void synth_clear_matrix(Synth *synth)
{
    memset(synth->matrix, 0, sizeof synth->matrix);
    memset(synth->usb_routes, 0, sizeof synth->usb_routes);
    for (unsigned col = 0; col < MATRIX_SIZE; ++col) rebuild_column(synth, col);
}

bool synth_usb_route(Synth *synth, unsigned channel, unsigned col, enum Pin amount)
{
    if (channel >= 2 || col >= MATRIX_SIZE || destinations[col].stage < 0 || (unsigned)amount > PIN_HALF)
        return false;
    synth->usb_routes[channel][col] = (uint8_t)amount;
    rebuild_column(synth, col);
    return true;
}

bool synth_ks_route(Synth *synth, unsigned col, enum Pin amount)
{
    if (col >= MATRIX_SIZE || destinations[col].stage < 0 || (unsigned)amount > PIN_HALF) return false;
    synth->ks_routes[col] = (uint8_t)amount;
    rebuild_column(synth, col);
    return true;
}

void synth_ks_matrix(Synth *synth, bool enabled)
{
    if (synth->ks_matrix == enabled) return;
    synth->ks_matrix = enabled;
    for (unsigned col = 0; col < MATRIX_SIZE; ++col) rebuild_column(synth, col);
}

bool synth_pin(Synth *synth, unsigned row, unsigned col, enum Pin pin)
{
    if (!synth_pin_supported(row, col) || (unsigned)pin > PIN_HALF) return false;
    synth->matrix[row][col] = (uint8_t)pin;
    rebuild_column(synth, col);
    return true;
}

bool synth_toggle_pin(Synth *synth, unsigned row, unsigned col, enum Pin pin)
{
    if (!synth_pin_supported(row, col) || (pin != PIN_FULL && pin != PIN_HALF)) return false;
    return synth_pin(synth, row, col, synth->matrix[row][col] == pin ? PIN_OFF : pin);
}

bool synth_init(Synth *synth, ZiviReal rate)
{
    if (!isfinite(rate) || rate < 8000.0 || rate > ZIVI_MAX_RATE) return false;
    memset(synth, 0, sizeof *synth);
    synth->rate = rate;
    synth->pitch_note = 69;
    synth->noise = 0x79b125c3u;
    modules_init(&synth->modules, rate);
    ks_init(&synth->ks, (uint32_t)(rate + 0.5));
    synth_safe_patch(synth);
    synth->arp.random_state = 0xf1854c3du;
    synth_arp_scale(synth, 0, SCALE_CHROMATIC);
    synth_arp_modes(synth, false, false, 100);
    synth_arp(synth, false, 120, 60, ARP_UP, ARP_EIGHTH);
    synth->sequencer.division = ARP_SIXTEENTH;
    synth_seq_timing(synth, 16, 120);
    return true;
}

void synth_safe_patch(Synth *synth)
{
    synth->ks_matrix = false;
    memset(synth->ks_routes, 0, sizeof synth->ks_routes);
    synth_clear_matrix(synth);
    synth_pin(synth, SRC_OSC1, DST_ENV_IN, PIN_FULL);
    synth_pin(synth, SRC_ENVELOPE, DST_OUT1_IN, PIN_FULL);
    synth_pin(synth, SRC_ENVELOPE, DST_OUT2_IN, PIN_FULL);
    synth_pin(synth, SRC_INPUT1, DST_OSC1_PITCH, PIN_FULL);
    synth->tune = 0.0;
    synth->shape = 0.5;
    synth->level = 0.7;
    synth->mix = 0.0;
    synth->lfo = 3.5;
    synth->cutoff = 1200.0;
    synth->ks.sequence_trigger = false;
    modules_defaults(&synth->modules, synth->rate);
}

void synth_ks_patch(Synth *synth)
{
    synth_safe_patch(synth);
    synth_pin(synth, SRC_INPUT1, DST_OSC1_PITCH, PIN_OFF);
    synth_ks_route(synth, DST_OSC1_PITCH, PIN_FULL);
    synth_pin(synth, SRC_ENVELOPE, DST_OUT2_IN, PIN_OFF);
    synth_pin(synth, SRC_INPUT1, DST_OSC2_PITCH, PIN_FULL);
    synth_pin(synth, SRC_OSC2, DST_OUT2_IN, PIN_FULL);
    synth_pin(synth, SRC_INPUT2, DST_OUT2_LEVEL, PIN_FULL);
    synth->ks.sequence_trigger = true;
}

void synth_spring_patch(Synth *synth)
{
    synth_safe_patch(synth);
    synth_pin(synth, SRC_ENVELOPE, DST_OUT1_IN, PIN_HALF);
    synth_pin(synth, SRC_ENVELOPE, DST_OUT2_IN, PIN_OFF);
    synth_pin(synth, SRC_ENVELOPE, DST_REVERB_IN, PIN_FULL);
    synth_pin(synth, SRC_REVERB, DST_OUT2_IN, PIN_FULL);
    synth_patch_controls(synth, 0.28, synth->lfo, synth->cutoff);
    synth_module_control(synth, M4_ENV_MODE, 1.0);
    synth_module_control(synth, M4_ATTACK_MS, 10.0);
    synth_module_control(synth, M4_ON_MS, 65.0);
    synth_module_control(synth, M4_DECAY_MS, 330.0);
}

void synth_repeat_patch(Synth *synth)
{
    synth_spring_patch(synth);
    synth_pin(synth, SRC_OSC1, DST_ENV_IN, PIN_OFF);
    synth_pin(synth, SRC_OSC1, DST_FILTER_IN, PIN_FULL);
    synth_pin(synth, SRC_FILTER, DST_ENV_IN, PIN_FULL);
    synth_pin(synth, SRC_TRAPEZOID, DST_FILTER_CUTOFF, PIN_HALF);
    synth_pin(synth, SRC_ENVELOPE, DST_OUT1_IN, PIN_FULL);
    synth_pin(synth, SRC_REVERB, DST_OUT2_IN, PIN_HALF);
    synth_patch_controls(synth, 0.78, synth->lfo, 390.0);
    synth_module_control(synth, M4_RESPONSE, 0.62);
    synth_module_control(synth, M4_REVERB_MIX, 0.86);
    synth_module_control(synth, M4_ATTACK_MS, 80);
    synth_module_control(synth, M4_ON_MS, 100);
    synth_module_control(synth, M4_DECAY_MS, 320);
    synth_module_control(synth, M4_OFF_MS, 290);
    synth_module_control(synth, M4_ENV_MODE, 2);
}

void synth_patch_controls(Synth *synth, ZiviReal mix, ZiviReal lfo, ZiviReal cutoff)
{
    if (isfinite(mix)) {
        mix = clamp(mix, 0.0, 1.0);
        if (mix != synth->mix) {
            synth->mix = mix;
            synth_module_control(synth, M4_SINE_LEVEL, 1.0 - mix);
            synth_module_control(synth, M4_RAMP_LEVEL, mix);
        }
    }
    if (isfinite(lfo)) synth->lfo = clamp(lfo, 0.02, 500.0);
    if (isfinite(cutoff)) synth->cutoff = clamp(cutoff, 5.0, 10000.0);
}

void synth_controls(Synth *synth, ZiviReal tune, ZiviReal shape, ZiviReal level)
{
    if (isfinite(tune)) synth->tune = clamp(tune, -24.0, 24.0);
    if (isfinite(shape)) synth->shape = clamp(shape, 0.0, 1.0);
    if (isfinite(level)) synth->level = clamp(level, 0.0, 1.0);
}

void synth_module_control(Synth *synth, unsigned param, ZiviReal value)
{
    modules_set(&synth->modules, synth->rate, param, value);
}

void synth_manual_attack(Synth *synth)
{
    synth->modules.manual_attack = true;
}

void synth_manual_hold(Synth *synth, bool down)
{
    if (down && !synth->modules.manual_hold) synth_manual_attack(synth);
    synth->modules.manual_hold = down;
}

void synth_stick_x(Synth *synth, ZiviReal value)
{
    if (isfinite(value)) synth->stick_x = clamp(value, -1.0, 1.0);
}

void synth_stick_y(Synth *synth, ZiviReal value)
{
    if (isfinite(value)) synth->stick_y = clamp(value, -1.0, 1.0);
}

static unsigned live_note(const Synth *synth)
{
    if (synth->ks.random_held) return synth->ks.random_note;
    return synth->held_count ? synth->held[synth->held_count - 1] : synth->pitch_note;
}

static unsigned live_velocity(const Synth *synth)
{
    if (synth->ks.random_held) return 100;
    return synth->held_count ? synth->velocity[synth->held[synth->held_count - 1]] : 0;
}

unsigned synth_scale_note(unsigned note, unsigned root, enum ScaleMode mode)
{
    static const uint16_t masks[SCALE_COUNT] = {
#define SCALE(symbol, mask, name, lcd, group) [SCALE_##symbol] = mask,
#include "scales.def"
#undef SCALE
    };
    if (note >= MIDI_NOTES || root >= 12 || (unsigned)mode >= SCALE_COUNT || mode == SCALE_CHROMATIC) return note;
    for (unsigned distance = 0; distance < 12; ++distance) {
        if (note >= distance && (masks[mode] & (1u << ((note - distance + 12 - root) % 12))))
            return note - distance;
        if (note + distance < MIDI_NOTES && (masks[mode] & (1u << ((note + distance + 12 - root) % 12))))
            return note + distance;
    }
    return note;
}

static uint32_t division_period(uint32_t rate, unsigned bpm, enum ArpDivision division)
{
    bool triplet = (division & 1u) != 0;
    unsigned divisor = bpm * (1u << (division / 2u)) * (triplet ? 3u : 1u);
    return (rate * (triplet ? 120u : 60u) + divisor / 2u) / divisor;
}

void synth_arp(Synth *synth, bool enabled, unsigned bpm, unsigned gate, enum ArpOrder order, enum ArpDivision division)
{
    Arp *arp = &synth->arp;
    bpm = bpm < 30 ? 30 : bpm > 300 ? 300 : bpm;
    gate = gate < 10 ? 10 : gate > 95 ? 95 : gate;
    if ((unsigned)order >= ARP_ORDER_COUNT) order = ARP_UP;
    if ((unsigned)division >= ARP_DIVISION_COUNT) division = ARP_EIGHTH;
    if (arp->enabled == enabled && arp->bpm == bpm && arp->gate == gate &&
        arp->order == order && arp->division == division) return;
    bool was_enabled = arp->enabled;
    arp->period = division_period((uint32_t)synth->rate, synth->clock_bpm ? synth->clock_bpm : bpm, division);
    arp->division = division;
    arp->gate_frames = arp->period * gate / 100u;
    arp->bpm = bpm;
    arp->gate = gate;
    arp->order = order;
    arp->phase = arp->index = arp->velocity = 0;
    arp->note = arp->source_note = MIDI_NOTES;
    arp->enabled = enabled;
    if (!was_enabled && !enabled) return;
    synth->live_attack = !enabled && live_velocity(synth) != 0;
    if (synth->ks.mode == KS_RECORDING)
        ks_capture(&synth->ks, live_note(synth), enabled ? 0 : live_velocity(synth), synth->live_attack);
}

void synth_arp_scale(Synth *synth, unsigned root, enum ScaleMode mode)
{
    if (root >= 12 || (unsigned)mode >= SCALE_COUNT) return;
    Arp *arp = &synth->arp;
    arp->root = root;
    arp->scale = mode;
    arp->scale_count = 0;
    unsigned tonic = 60 + root;
    for (unsigned note = tonic; note < tonic + 12; ++note) {
        if (synth_scale_note(note, root, mode) == note)
            arp->scale_notes[arp->scale_count++] = (uint8_t)note;
    }
}

void synth_arp_modes(Synth *synth, bool hold, bool free_run, unsigned chance)
{
    Arp *arp = &synth->arp;
    if (hold != arp->hold) {
        arp->latched_count = hold ? (unsigned)synth->held_count : 0;
        if (hold) {
            memcpy(arp->latched, synth->held, synth->held_count);
            memcpy(arp->latched_velocity, synth->velocity, sizeof arp->latched_velocity);
        }
    }
    arp->hold = hold;
    arp->free_run = free_run;
    arp->chance = chance > 100 ? 100 : chance;
}

static unsigned arp_random(Arp *arp, unsigned range)
{
    uint32_t state = arp->random_state;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    arp->random_state = state;
    return state % range;
}

static unsigned arp_next(Arp *arp, const uint8_t *notes, unsigned count)
{
    if (arp->order == ARP_RANDOM) return notes[arp_random(arp, count)];
    if (arp->order == ARP_PLAYED) {
        if (arp->index >= count) arp->index = 0;
        return notes[arp->index++];
    }
    unsigned wrap = notes[0], next = MIDI_NOTES;
    bool ascending = arp->order == ARP_UP;
    for (unsigned i = 0; i < count; ++i) {
        unsigned note = notes[i];
        if (ascending ? note < wrap : note > wrap) wrap = note;
        bool follows = ascending ? note > arp->source_note : note < arp->source_note;
        if (arp->source_note < MIDI_NOTES && follows &&
            (next == MIDI_NOTES || (ascending ? note < next : note > next))) next = note;
    }
    return next == MIDI_NOTES ? wrap : next;
}

static bool arp_sample(Synth *synth)
{
    Arp *arp = &synth->arp;
    bool latched = arp->hold && arp->latched_count;
    bool generated = !latched && !synth->held_count && arp->free_run;
    unsigned count = latched ? arp->latched_count : generated ? arp->scale_count : (unsigned)synth->held_count;
    if (!count) {
        arp->phase = arp->index = arp->velocity = 0;
        arp->note = arp->source_note = MIDI_NOTES;
        return false;
    }
    bool attack = arp->phase == 0;
    if (attack) {
        const uint8_t *notes = latched ? arp->latched : generated ? arp->scale_notes : synth->held;
        unsigned source = arp_next(arp, notes, count);
        arp->source_note = source;
        arp->note = generated ? source : synth_scale_note(source, arp->root, arp->scale);
        bool audible = arp->chance == 100 || (arp->chance && arp_random(arp, 100) < arp->chance);
        arp->velocity = audible ? (generated ? 100 : latched ? arp->latched_velocity[source] : synth->velocity[source]) : 0;
    } else if (arp->phase >= arp->gate_frames) {
        arp->velocity = 0;
    }
    if (++arp->phase == arp->period) arp->phase = 0;
    return attack && arp->velocity != 0;
}

static uint32_t seq_frames(const StepSequencer *seq, uint32_t frames)
{
    /* Divide first to keep the audio target on native 32-bit arithmetic. */
    unsigned bpm = seq->clock_bpm ? seq->clock_bpm : seq->bpm;
    return (frames / bpm) * seq->timing_bpm
         + ((frames % bpm) * seq->timing_bpm + bpm / 2u) / bpm;
}

static uint32_t seq_span(const StepSequencer *seq, const SeqStep *step)
{
    uint32_t span = step->span ? seq_frames(seq, step->span) : seq->period;
    return span ? span : 1;
}

static uint32_t seq_delay(const StepSequencer *seq, const SeqStep *step, uint32_t span)
{
    uint32_t delay = step->span ? seq_frames(seq, step->delay) : 0;
    return delay < span ? delay : span - 1;
}

static uint32_t seq_end(const StepSequencer *seq, const SeqStep *step, uint32_t delay)
{
    uint32_t end = step->span ? seq_frames(seq, step->delay + step->duration) : seq->period * 3u / 4u;
    return end > delay ? end : delay + 1;
}

static void seq_step_timing(StepSequencer *seq)
{
    const SeqStep *step = &seq->steps[seq->index];
    seq->step_period = seq_span(seq, step);
    seq->delay = seq_delay(seq, step, seq->step_period);
    seq->end = seq_end(seq, step, seq->delay);
}

void synth_seq_timing(Synth *synth, unsigned length, unsigned bpm)
{
    StepSequencer *seq = &synth->sequencer;
    unsigned low = seq->division == ARP_SIXTEENTH ? 40 : 30;
    seq->length = length < 1 ? 1 : length > SEQ_STEPS ? SEQ_STEPS : length;
    seq->bpm = bpm < low ? low : bpm > 300 ? 300 : bpm;
    seq->period = division_period((uint32_t)synth->rate, seq->clock_bpm ? seq->clock_bpm : seq->bpm, seq->division);
    if (seq->index >= seq->length) {
        seq->index %= seq->length;
        seq->phase = 0;
        seq->velocity = 0;
        seq->captured = false;
    }
    seq_step_timing(seq);
    if (seq->phase >= seq->step_period) {
        seq->phase = seq->velocity = 0;
        seq->captured = false;
    }
}

void synth_seq_play(Synth *synth, bool playing)
{
    StepSequencer *seq = &synth->sequencer;
    if (playing) ks_stop(&synth->ks);
    seq->playing = playing;
    seq->recording = seq->captured = seq->seek_attack = false;
    seq->phase = seq->index = seq->velocity = 0;
    seq->note = synth->pitch_note;
    seq_step_timing(seq);
}

void synth_seq_seek(Synth *synth, uint32_t frames)
{
    StepSequencer *seq = &synth->sequencer;
    uint32_t length = 0;
    for (unsigned i = 0; i < seq->length; ++i) length += seq_span(seq, &seq->steps[i]);
    synth_seq_play(synth, true);
    frames %= length;
    while (frames >= seq->step_period) {
        frames -= seq->step_period;
        ++seq->index;
        seq_step_timing(seq);
    }
    seq->phase = frames;
    if (frames > seq->delay && frames < seq->end) {
        const SeqStep *step = &seq->steps[seq->index];
        seq->velocity = step->velocity;
        if (seq->velocity) {
            seq->note = step->note;
            seq->seek_attack = true;
        }
    }
}

static uint32_t tempo_phase(uint32_t phase, uint32_t before, uint32_t after)
{
    return phase < before ? (uint32_t)((ZiviReal)phase * after / before) : after;
}

void synth_sync_tempo(Synth *synth, unsigned bpm)
{
    if (bpm) bpm = bpm < 30 ? 30 : bpm > 300 ? 300 : bpm;
    if (synth->clock_bpm == bpm && (synth->sequencer.recording || synth->sequencer.clock_bpm == bpm)) return;
    synth->clock_bpm = bpm;
    Arp *arp = &synth->arp;
    uint32_t period = division_period((uint32_t)synth->rate, bpm ? bpm : arp->bpm, arp->division);
    arp->phase = tempo_phase(arp->phase, arp->period, period);
    if (arp->phase >= period) arp->phase = 0;
    arp->period = period;
    arp->gate_frames = period * arp->gate / 100u;

    StepSequencer *seq = &synth->sequencer;
    if (seq->recording) return;
    uint32_t old_period = seq->step_period;
    uint32_t old_phase = seq->phase;
    seq->clock_bpm = bpm;
    seq->period = division_period((uint32_t)synth->rate, bpm ? bpm : seq->bpm, seq->division);
    seq_step_timing(seq);
    seq->phase = tempo_phase(old_phase, old_period, seq->step_period);
}

void synth_seq_record(Synth *synth)
{
    StepSequencer *seq = &synth->sequencer;
    const Arp *arp = &synth->arp;
    synth_seq_play(synth, true);
    /* Capture a new take on the active grid. Silence and missed ARP gates become rests. */
    memset(seq->steps, 0, sizeof seq->steps);
    seq->division = arp->enabled ? arp->division : ARP_SIXTEENTH;
    synth_seq_timing(synth, seq->length, arp->enabled ? arp->bpm : seq->bpm);
    seq->timing_bpm = seq->clock_bpm ? seq->clock_bpm : seq->bpm;
    seq->recording = true;
    synth->arp.phase = synth->arp.index = synth->arp.velocity = 0;
    synth->arp.note = synth->arp.source_note = MIDI_NOTES;
    synth->live_attack = synth->held_count != 0;
}

bool synth_seq_set(Synth *synth, unsigned index, unsigned note, unsigned velocity)
{
    if (index >= SEQ_STEPS || note >= MIDI_NOTES || velocity >= 128) return false;
    SeqStep *step = &synth->sequencer.steps[index];
    step->note = (uint8_t)note;
    step->velocity = (uint8_t)velocity;
    return true;
}

bool synth_seq_from_ks(Synth *synth)
{
    const Ks *ks = &synth->ks;
    if (!ks->count || !ks->length || ks->mode == KS_RECORDING) return false;
    unsigned count = 0;
    for (unsigned i = 0; i < ks->count; ++i) count += ks->events[i].velocity != 0;
    if (!count || count > SEQ_STEPS) return false;

    StepSequencer *seq = &synth->sequencer;
    memset(seq->steps, 0, sizeof seq->steps);
    unsigned cell = 0;
    for (unsigned i = 0; i < ks->count; ++i) {
        const KsEvent *event = &ks->events[i];
        if (!event->velocity) continue;
        uint32_t next_on = ks->length, off = ks->length;
        for (unsigned j = i + 1; j < ks->count; ++j) {
            const KsEvent *next = &ks->events[j];
            if (off == ks->length) off = next->frame;
            if (next->velocity) {
                next_on = next->frame;
                break;
            }
        }
        uint32_t start = cell ? event->frame : 0;
        seq->steps[cell++] = (SeqStep){.span = next_on - start, .delay = event->frame - start,
            .duration = off - event->frame, .note = event->note, .velocity = event->velocity,
            .legato = !event->attack};
    }
    seq->division = ARP_SIXTEENTH;
    unsigned bpm = (ks->rate * 15u * count + ks->length / 2u) / ks->length;
    seq->timing_bpm = bpm < 40 ? 40 : bpm > 300 ? 300 : bpm;
    synth_seq_timing(synth, count, seq->timing_bpm);
    synth_seq_play(synth, false);
    return true;
}

bool synth_seq_to_ks(Synth *synth)
{
    StepSequencer *seq = &synth->sequencer;
    Ks *ks = &synth->ks;
    if (seq->recording || ks->mode == KS_RECORDING) return false;
    unsigned count = 0;
    uint32_t length = 0;
    for (unsigned i = 0; i < seq->length; ++i) {
        const SeqStep *step = &seq->steps[i];
        count += step->velocity != 0;
        length += seq_span(seq, step);
    }
    if (!count || length > ks->rate * 30u) return false;

    ks_stop(ks);
    ks->count = 0;
    uint32_t at = 0;
    for (unsigned i = 0; i < seq->length; ++i) {
        const SeqStep *step = &seq->steps[i];
        uint32_t span = seq_span(seq, step);
        if (step->velocity) {
            uint32_t delay = seq_delay(seq, step, span);
            uint32_t end = seq_end(seq, step, delay);
            bool contiguous = ks->count && ks->events[ks->count - 1].velocity;
            ks->events[ks->count++] = (KsEvent){at + delay, step->note, step->velocity, !contiguous || !step->legato};
            const SeqStep *next_step = step + 1;
            bool next = i + 1 < seq->length && next_step->velocity &&
                        !seq_delay(seq, next_step, seq_span(seq, next_step));
            if (end < span || !next) {
                uint32_t off = at + (end < span ? end : span);
                if (off < length)
                    ks->events[ks->count++] = (KsEvent){off, step->note, 0, false};
            }
        }
        at += span;
    }
    ks->length = length;
    ks->limit = KS_LIMIT_NONE;
    return true;
}

static void seq_advance(StepSequencer *seq)
{
    if (seq->phase < seq->step_period) return;
    if (++seq->index >= seq->length) seq->index = 0;
    seq->phase = 0;
    seq->captured = false;
    seq_step_timing(seq);
}

static bool seq_sample(StepSequencer *seq)
{
    seq_advance(seq);
    bool attack = seq->seek_attack;
    seq->seek_attack = false;
    if (seq->phase == seq->delay) {
        const SeqStep *step = &seq->steps[seq->index];
        attack = step->velocity && (!step->legato || !seq->velocity);
        seq->velocity = step->velocity;
        if (seq->velocity) seq->note = step->note;
    } else if (seq->phase < seq->delay || seq->phase >= seq->end) {
        seq->velocity = 0;
    }
    ++seq->phase;
    return attack;
}

static void seq_capture(StepSequencer *seq, unsigned note, unsigned velocity, bool attack)
{
    bool boundary = seq->phase >= seq->step_period;
    bool legato = false;
    if (boundary && !attack && seq->captured) {
        const SeqStep *prior = &seq->steps[seq->index];
        legato = prior->note == note && prior->delay + prior->duration == seq->phase;
    }
    seq_advance(seq);
    SeqStep *step = &seq->steps[seq->index];
    if (!seq->phase) *step = (SeqStep){0};
    if ((attack || boundary) && velocity && !seq->captured) {
        *step = (SeqStep){.span = seq->period, .delay = seq->phase, .legato = legato,
                         .note = (uint8_t)note, .velocity = (uint8_t)velocity};
        seq->captured = true;
    }
    if (seq->captured && velocity && note == step->note && step->delay + step->duration == seq->phase)
        ++step->duration;
    if (++seq->phase >= seq->step_period && seq->index + 1 >= seq->length) {
        /* Finish one pass. Playback starts at step one on the next render block. */
        seq->recording = false;
        seq->phase = seq->index = seq->velocity = 0;
        seq_step_timing(seq);
    }
}

static ZiviReal pitch_cv(const Synth *synth, unsigned note, ZiviReal spread)
{
    return clamp(((ZiviReal)note - 69.0) * spread / 48.0
                 + synth->bend / 48.0, -1.5, 1.5);
}

void synth_ks_record(Synth *synth)
{
    if (synth->sequencer.playing) synth_seq_play(synth, false);
    ks_record(&synth->ks);
    if (!synth->arp.enabled || synth->ks.random_held)
        ks_capture(&synth->ks, live_note(synth), live_velocity(synth), true);
}

void synth_ks_random(Synth *synth, bool down)
{
    if (synth->ks.random_held == down) return;
    unsigned prior_note = live_note(synth);
    ks_random(&synth->ks, down);
    if (down) synth->live_attack = true;
    if (!synth->held_count) synth->pitch_note = prior_note;
    ks_capture(&synth->ks, live_note(synth), live_velocity(synth), down);
}

void synth_all_notes_off(Synth *synth)
{
    unsigned prior_note = live_note(synth);
    memset(synth->velocity, 0, sizeof synth->velocity);
    synth->ks.random_held = false;
    synth->live_attack = false;
    synth->pitch_note = prior_note;
    ks_capture(&synth->ks, prior_note, 0, false);
    synth->held_count = 0;
    synth->arp.latched_count = 0;
    synth->arp.free_run = false;
    synth->arp.velocity = synth->arp.phase = 0;
    synth->bend = 0.0;
}

void synth_midi_channel(Synth *synth, unsigned channel)
{
    if (channel <= 16 && channel != synth->midi_channel) {
        synth_all_notes_off(synth);
        synth->midi_channel = channel;
    }
}

static void note_event(Synth *synth, unsigned note, unsigned velocity)
{
    unsigned prior_note = live_note(synth);
    bool first_key = synth->held_count == 0;
    for (size_t i = 0; i < synth->held_count; ++i) {
        if (synth->held[i] == note) {
            --synth->held_count;
            for (unsigned j = i; j < synth->held_count; ++j) {
                synth->held[j] = synth->held[j + 1];
            }
            break;
        }
    }
    synth->velocity[note] = (uint8_t)velocity;
    if (velocity != 0) {
        synth->held[synth->held_count++] = (uint8_t)note;
        Arp *arp = &synth->arp;
        if (arp->hold) {
            if (first_key) {
                arp->latched_count = 0;
                memset(arp->latched_velocity, 0, sizeof arp->latched_velocity);
            }
            if (!arp->latched_velocity[note]) arp->latched[arp->latched_count++] = (uint8_t)note;
            arp->latched_velocity[note] = (uint8_t)velocity;
        }
    }
    if (!synth->held_count && !synth->ks.random_held) synth->pitch_note = prior_note;
    bool attack = velocity != 0 && !synth->ks.random_held;
    if (attack) synth->live_attack = true;
    if (!synth->arp.enabled || synth->ks.random_held)
        ks_capture(&synth->ks, live_note(synth), live_velocity(synth), attack);
}

bool synth_midi(Synth *synth, unsigned status, unsigned data1, unsigned data2)
{
    if (status < 0x80 || status > 0xEF || data1 >= 128 || data2 >= 128) return false;
    if (synth->midi_channel && synth->midi_channel != (status & 15u) + 1u) return false;
    switch (status & 0xF0u) {
    case 0x80:
        note_event(synth, data1, 0);
        return true;
    case 0x90:
        note_event(synth, data1, data2);
        return true;
    case 0xB0:
        if (data1 == 16) synth_stick_x(synth, (data2 - 64.0) / (data2 >= 64 ? 63.0 : 64.0));
        else if (data1 == 17) synth_stick_y(synth, (data2 - 64.0) / (data2 >= 64 ? 63.0 : 64.0));
        else if (data1 == 1) synth->shape = data2 / 127.0;
        else if (data1 == 7) synth->level = data2 / 127.0;
        else if (data1 == 120 || data1 == 123) synth_all_notes_off(synth);
        else return false;
        return true;
    case 0xE0: {
        int centered = (int)(data1 | (data2 << 7)) - 8192;
        synth->bend = centered * 2.0 / (centered > 0 ? 8191.0 : 8192.0);
        return true;
    }
    default:
        return false;
    }
}

static ZiviReal poly_blep(ZiviReal phase, ZiviReal step)
{
    if (phase < step) {
        ZiviReal t = phase / step;
        return t + t - t * t - 1.0;
    }
    if (phase > 1.0 - step) {
        ZiviReal t = (phase - 1.0) / step;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

/* Esta soma é independente da ordem de edição dos pinos. Nos caminhos
 * atrasados a amostra anterior é lida antes de qualquer escrita de estado. */
static ZiviReal bus(const Synth *synth, const ZiviReal *signals, const ZiviReal *previous, unsigned col)
{
    ZiviReal sum = 0.0;
    for (unsigned i = 0; i < synth->counts[col]; ++i) {
        const Route *route = &synth->routes[col][i];
        const ZiviReal *values = route->delayed ? previous : signals;
        sum += values[route->source] * route->gain;
    }
    return col == DST_OSC1_PITCH || col == DST_OSC2_PITCH || col == DST_OSC3_PITCH
        ? clamp(sum, -1.5, 1.5) : clamp(sum, -1.0, 1.0);
}

static ZiviReal advance(ZiviReal phase, ZiviReal step)
{
    phase += step;
    return phase >= 1.0 ? phase - 1.0 : phase;
}

#define MASTER_GAIN 0.24

static ZiviReal pulse(ZiviReal phase, ZiviReal step, ZiviReal duty)
{
    ZiviReal shifted = phase - duty;
    if (shifted < 0.0) shifted += 1.0;
    return (phase < duty ? 1.0 : -1.0) + poly_blep(phase, step) - poly_blep(shifted, step);
}

#ifdef ZIVI_SMK_OPEN_AUDIO
static inline ZiviReal triangle_shaped(ZiviReal phase, ZiviReal duty, ZiviReal rise, ZiviReal fall)
{
    return phase < duty ? rise * phase - 1.0 : 1.0 - fall * (phase - duty);
}
#else
static ZiviReal triangle(ZiviReal phase, ZiviReal duty)
{
    return phase < duty ? 2.0 * phase / duty - 1.0
                        : 1.0 - 2.0 * (phase - duty) / (1.0 - duty);
}
#endif

static void render_inputs(Synth *synth, float *left, float *right, size_t frames,
                        const float *input1, const float *input2, bool usb)
{
    Modules *m = &synth->modules;
    const ZiviReal *p = m->p;
    unsigned note = live_note(synth);
    synth->pitch_note = note;
    unsigned key_velocity = live_velocity(synth);
    ZiviReal velocity = key_velocity / 127.0;
    StepSequencer *step_seq = &synth->sequencer;
    bool step_playing = step_seq->playing && !step_seq->recording;
    const ZiviReal realtime_spread = synth->ks.realtime_spread / 100.0;
    bool arpeggiated = synth->arp.enabled && !synth->ks.random_held;
    unsigned arp_note = arpeggiated && synth->arp.note < MIDI_NOTES ? synth->arp.note : note;
    if (step_playing) arp_note = step_seq->note;
    ZiviReal pitch = pitch_cv(synth, arp_note, step_playing ? 1.0 : realtime_spread);
    unsigned arp_velocity = synth->arp.velocity;
    ZiviReal sequence_spread = synth->ks.sequence_spread / 100.0;
    ZiviReal asymmetry = 0.15 + 0.70 * synth->shape;
    ZiviReal duty2 = 0.05 + 0.90 * p[M4_OSC2_SHAPE];
    ZiviReal duty3 = 0.05 + 0.90 * p[M4_OSC3_SHAPE];
#ifdef ZIVI_SMK_OPEN_AUDIO
    /* Shape controls are constant during one render call. Avoid divisions
     * for every sample of the warped sine and the two triangle waves. */
    const ZiviReal warp_left = 0.5 / asymmetry, warp_right = 0.5 / (1.0 - asymmetry);
    const ZiviReal rise2 = 2.0 / duty2, fall2 = 2.0 / (1.0 - duty2);
    const ZiviReal rise3 = 2.0 / duty3, fall3 = 2.0 / (1.0 - duty3);
#endif
    ZiviReal phase = synth->phase, osc2_phase = synth->osc2_phase, lfo_phase = synth->lfo_phase;
    ZiviReal step = 0.0, osc2_step = 0.0, lfo_step = 0.0, coef = 0.0, meter = 0.0;
    ZiviReal prior_pitch = 2.0, prior_osc2 = 2.0, prior_lfo = 2.0, prior_cutoff = 2.0;
    ZiviReal effective_cutoff = 0.0;
    ZiviReal pan_a = (p[M4_OUT1_PAN] + 1.0) * 0.5;
    ZiviReal pan_c = (p[M4_OUT2_PAN] + 1.0) * 0.5;
    /* The matrix reads the previous sample until all current sources are
     * complete. Swap frames here and persist the final one once per block. */
    ZiviReal next_frame[ROUTE_SOURCES] = {0};
    ZiviReal *signals = next_frame, *previous = synth->signals;
    const bool aks = m->aks_profile;
    const ZiviReal osc_limit = synth->rate * (aks ? 0.25 : 0.22);
    const ZiviReal osc_audio_sensitivity = aks ? aks_cv_reference / aks_osc_audio_octave : 4.0;
    const ZiviReal osc_low_sensitivity = aks ? aks_cv_reference / aks_osc_low_octave : 4.0;
    const ZiviReal filter_sensitivity = aks ? aks_cv_reference / aks_filter_octave : 4.0;
    const ZiviReal stick_scale = aks ? aks_stick_volts / aks_cv_reference : 1.0;
    const ZiviReal lfo_limit = synth->rate * 0.18;
    const ZiviReal stick_x = synth->stick_x * stick_scale, stick_y = synth->stick_y * stick_scale;
    const bool ks_matrix = synth->ks_matrix;
    const ZiviReal noise_color = p[M4_NOISE_COLOR];
    const ZiviReal lighter = dsp_max(0.0, noise_color);
    const ZiviReal darker = dsp_min(0.0, noise_color);
    const ZiviReal pan_left_a = 1.0 - pan_a, pan_left_c = 1.0 - pan_c;

    for (size_t i = 0; i < frames; ++i) {
        ks_prepare_sample(&synth->ks);
        bool recording = synth->ks.mode == KS_RECORDING;
        bool live_trigger = synth->live_attack;
        if (step_playing) {
            live_trigger = seq_sample(step_seq);
            if (step_seq->note != arp_note) {
                arp_note = step_seq->note;
                pitch = pitch_cv(synth, arp_note, 1.0);
            }
            velocity = step_seq->velocity / 127.0;
        } else if (arpeggiated) {
            live_trigger = arp_sample(synth);
            const Arp *arp = &synth->arp;
            if (arp->note < MIDI_NOTES && arp->note != arp_note) {
                arp_note = arp->note;
                pitch = pitch_cv(synth, arp_note, realtime_spread);
            }
            velocity = arp->velocity / 127.0;
            if (recording && (live_trigger || arp->velocity != arp_velocity))
                ks_capture(&synth->ks, arp_note, arp->velocity, live_trigger);
            arp_velocity = arp->velocity;
        }
        bool step_recording = step_seq->recording;
        if (step_recording)
            seq_capture(step_seq, arp_note, arpeggiated ? arp_velocity : key_velocity, live_trigger);
        ZiviReal seq_velocity = recording ? velocity : synth->ks.velocity / 127.0;
        ZiviReal seq_note = (recording || step_playing || step_recording) ? arp_note : synth->ks.note;
        ZiviReal target = synth->ks.sequence_trigger && !step_playing && !step_recording ? seq_velocity : velocity;
        bool trigger = synth->ks.sequence_trigger && !recording && !step_playing && !step_recording ? synth->ks.attack : live_trigger;
        synth->live_attack = false;
        ZiviReal threshold = dsp_abs(synth->threshold_probe);
        bool high = threshold >= (synth->threshold_active ? 0.10 : 0.18);
        if (p[M4_TRIGGER_MODE] == 1.0) {
            target = high ? 1.0 : 0.0;
            trigger = high && !synth->threshold_active;
        }
        synth->threshold_active = high;

        synth->noise ^= synth->noise << 13;
        synth->noise ^= synth->noise >> 17;
        synth->noise ^= synth->noise << 5;
        ZiviReal noise = (synth->noise >> 8) / 8388608.0 - 1.0;
        m->noise_low += m->noise_coeff * (noise - m->noise_low);
        signals[SRC_NOISE] = (noise - lighter * m->noise_low
                            + darker * (noise - m->noise_low)) * p[M4_NOISE_LEVEL];
        ZiviReal external1 = input1 && isfinite(input1[i]) ? clamp(input1[i], -1.0, 1.0) : 0.0;
        ZiviReal external2 = input2 && isfinite(input2[i]) ? clamp(input2[i], -1.0, 1.0) : 0.0;
        ZiviReal in1 = usb || !input1 ? pitch : external1;
        ZiviReal in2 = usb || !input2 ? velocity : external2;
        signals[SRC_USB_L] = usb ? external1 : 0.0;
        signals[SRC_USB_R] = usb ? external2 : 0.0;
        signals[SRC_INPUT1] = clamp(in1 * p[M4_IN1_LEVEL], -1.5, 1.5);
        signals[SRC_INPUT2] = clamp(in2 * p[M4_IN2_LEVEL], -1.5, 1.5);
        signals[SRC_KS_INTERNAL] = (step_playing || step_recording) ? pitch : clamp(((seq_note - 69.0 + synth->ks.transpose) * sequence_spread) / 48.0,
                                         -1.5, 1.5);
        signals[SRC_STICK_X] = stick_x;
        signals[SRC_STICK_Y] = ks_matrix ? signals[SRC_KS_INTERNAL] : stick_y;
        ZiviReal decay_cv = bus(synth, signals, previous, DST_ENV_DECAY);
        ZiviReal envelope = modules_envelope(m, synth->rate, target, trigger, decay_cv);
        signals[SRC_TRAPEZOID] = envelope * p[M4_TRAP_LEVEL];

        ZiviReal pitch_mod = bus(synth, signals, previous, DST_OSC1_PITCH);
        if (pitch_mod != prior_pitch) {
            ZiviReal hz = p[M4_OSC1_HZ] * dsp_exp2((synth->tune + 48.0 * pitch_mod) / 12.0);
            step = clamp(hz, 0.1, osc_limit) / synth->rate;
            prior_pitch = pitch_mod;
        }
        ZiviReal osc2_mod = bus(synth, signals, previous, DST_OSC2_PITCH);
        if (osc2_mod != prior_osc2) {
            ZiviReal hz = p[M4_OSC2_HZ] * dsp_exp2(osc_audio_sensitivity * osc2_mod);
            osc2_step = clamp(hz, 0.1, osc_limit) / synth->rate;
            prior_osc2 = osc2_mod;
        }
        ZiviReal lfo_mod = bus(synth, signals, previous, DST_OSC3_PITCH);
        if (lfo_mod != prior_lfo) {
            ZiviReal hz = synth->lfo * dsp_exp2(osc_low_sensitivity * lfo_mod);
            lfo_step = clamp(hz, 0.02, lfo_limit) / synth->rate;
            prior_lfo = lfo_mod;
        }
#ifdef ZIVI_SMK_OPEN_AUDIO
        ZiviReal warped = phase < asymmetry ? warp_left * phase : 0.5 + warp_right * (phase - asymmetry);
        ZiviReal tri2 = triangle_shaped(osc2_phase, duty2, rise2, fall2);
        ZiviReal tri3 = triangle_shaped(lfo_phase, duty3, rise3, fall3);
#else
        ZiviReal warped = phase < asymmetry ? 0.5 * phase / asymmetry
                         : 0.5 + 0.5 * (phase - asymmetry) / (1.0 - asymmetry);
        ZiviReal tri2 = triangle(osc2_phase, duty2), tri3 = triangle(lfo_phase, duty3);
#endif
        ZiviReal ramp = 2.0 * phase - 1.0 - poly_blep(phase, step);
        signals[SRC_OSC1] = clamp(p[M4_SINE_LEVEL] * dsp_sine(warped) + p[M4_RAMP_LEVEL] * ramp, -1.0, 1.0);
        signals[SRC_OSC2] = clamp(p[M4_OSC2_TRI] * tri2
                              + p[M4_OSC2_PULSE] * pulse(osc2_phase, osc2_step, duty2), -1.0, 1.0);
        signals[SRC_OSC3_RECT] = p[M4_OSC3_PULSE] * pulse(lfo_phase, lfo_step, duty3);
        signals[SRC_OSC3_TRI] = p[M4_OSC3_TRI] * tri3;
        phase = advance(phase, step);
        osc2_phase = advance(osc2_phase, osc2_step);
        lfo_phase = advance(lfo_phase, lfo_step);

        ZiviReal cutoff_mod = bus(synth, signals, previous, DST_FILTER_CUTOFF);
        if (cutoff_mod != prior_cutoff) {
            effective_cutoff = clamp(synth->cutoff * dsp_exp2(filter_sensitivity * cutoff_mod), 5.0, osc_limit);
            coef = dsp_filter_g(effective_cutoff / synth->rate);
            prior_cutoff = cutoff_mod;
        }
        signals[SRC_FILTER] = modules_filter(m, bus(synth, signals, previous, DST_FILTER_IN), coef, noise);
        signals[SRC_ENVELOPE] = bus(synth, signals, previous, DST_ENV_IN) * envelope * p[M4_ENV_LEVEL];
        signals[SRC_RING] = bus(synth, signals, previous, DST_RING_A)
                          * bus(synth, signals, previous, DST_RING_B) * p[M4_RING_LEVEL];
        signals[SRC_REVERB] = modules_reverb(m, bus(synth, signals, previous, DST_REVERB_IN),
                                            bus(synth, signals, previous, DST_REVERB_MIX));

        ZiviReal gain_a = synth->counts[DST_OUT1_LEVEL] ? bus(synth, signals, previous, DST_OUT1_LEVEL) : 0.0;
        ZiviReal gain_c = synth->counts[DST_OUT2_LEVEL] ? bus(synth, signals, previous, DST_OUT2_LEVEL) : 0.0;
        if (!aks) {
            gain_a = synth->counts[DST_OUT1_LEVEL] ? dsp_max(0.0, gain_a) : 1.0;
            gain_c = synth->counts[DST_OUT2_LEVEL] ? dsp_max(0.0, gain_c) : 1.0;
        }
        ZiviReal a = modules_output(m, 0, bus(synth, signals, previous, DST_OUT1_IN), gain_a);
        synth->threshold_probe = bus(synth, signals, previous, DST_OUT2_IN);
        ZiviReal c = modules_output(m, 1, synth->threshold_probe, gain_c);
        signals[SRC_OUT1] = a;
        signals[SRC_OUT2] = c;
        meter = dsp_max(meter, dsp_abs(bus(synth, signals, previous, DST_METER)));
        ZiviReal *scratch = previous;
        previous = signals;
        signals = scratch;
        ZiviReal stereo_a = pan_left_a * a + pan_left_c * c;
        ZiviReal stereo_c = pan_a * a + pan_c * c;
        left[i] = (float)((right ? stereo_a : a + c) * MASTER_GAIN * synth->level);
        if (right) right[i] = (float)(stereo_c * MASTER_GAIN * synth->level);
        ks_advance(&synth->ks);
        if (step_recording && !step_seq->recording) {
            step_playing = true;
            arp_note = MIDI_NOTES;
        }
    }
    if (frames) {
        if (previous != synth->signals) memcpy(synth->signals, previous, sizeof synth->signals);
        synth->hz[0] = step * synth->rate;
        synth->hz[1] = osc2_step * synth->rate;
        synth->hz[2] = lfo_step * synth->rate;
        synth->hz[3] = effective_cutoff;
    }
    synth->phase = phase;
    synth->osc2_phase = osc2_phase;
    synth->lfo_phase = lfo_phase;
    if (m->env < 1e-12) m->env = 0.0;
    synth->meter_peak = meter;
}

void synth_render_input(Synth *synth, float *left, float *right, size_t frames,
                        const float *input1, const float *input2)
{
    render_inputs(synth, left, right, frames, input1, input2, false);
}

void synth_render_usb(Synth *synth, float *left, float *right, size_t frames,
                      const float *input1, const float *input2)
{
    render_inputs(synth, left, right, frames, input1, input2, true);
}

void synth_render_stereo(Synth *synth, float *left, float *right, size_t frames)
{
    synth_render_input(synth, left, right, frames, NULL, NULL);
}

void synth_render(Synth *synth, float *mono, size_t frames)
{
    synth_render_stereo(synth, mono, NULL, frames);
}
