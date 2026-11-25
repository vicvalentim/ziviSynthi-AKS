#ifndef ZIVI_SYNTH_H
#define ZIVI_SYNTH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ks.h"
#include "modules.h"

#ifndef ZIVI_VERSION
#ifndef ZIVI_VERSION
#define ZIVI_VERSION "0.7.1-alpha1"
#endif
#endif

enum { MATRIX_SIZE = 16, MATRIX_TILE = 4, MIDI_NOTES = 128 };
enum MatrixAddress {
#define SOURCE(id, index, label, stage) SRC_##id = index,
#define DEST(id, index, label, stage) DST_##id = index,
#include "matrix.def"
#undef SOURCE
#undef DEST
};
enum Pin { PIN_OFF, PIN_FULL, PIN_HALF };
/* The KS enters the route evaluator independently of the 16 × 16 patchboard. */
enum { SRC_KS_INTERNAL = MATRIX_SIZE, SRC_USB_L, SRC_USB_R, ROUTE_SOURCES };

typedef struct {
    uint8_t source;
    bool delayed;
    ZiviReal gain;
} Route;

enum ScaleMode {
#define SCALE(symbol, mask, name, lcd, group) SCALE_##symbol,
#include "scales.def"
#undef SCALE
    SCALE_COUNT
};
enum ArpOrder { ARP_UP, ARP_DOWN, ARP_PLAYED, ARP_RANDOM, ARP_ORDER_COUNT };
enum ArpDivision {
    ARP_QUARTER, ARP_QUARTER_TRIPLET, ARP_EIGHTH, ARP_EIGHTH_TRIPLET,
    ARP_SIXTEENTH, ARP_SIXTEENTH_TRIPLET, ARP_THIRTY_SECOND, ARP_THIRTY_SECOND_TRIPLET,
    ARP_DIVISION_COUNT
};
typedef struct {
    uint32_t phase, period, gate_frames;
    unsigned bpm, gate, index, note, velocity;
    enum ArpOrder order;
    enum ArpDivision division;
    uint32_t random_state;
    unsigned source_note, root, chance, latched_count, scale_count;
    enum ScaleMode scale;
    uint8_t latched[MIDI_NOTES], latched_velocity[MIDI_NOTES], scale_notes[12];
    bool enabled, hold, free_run;
} Arp;

enum { SEQ_STEPS = 64 };
typedef struct {
    uint32_t span, delay, duration; /* Frames at timing_bpm. Zero span uses the current grid. */
    uint8_t note, velocity;
    bool legato;
} SeqStep;

typedef struct {
    SeqStep steps[SEQ_STEPS];
    uint32_t phase, period, step_period, delay, end;
    unsigned length, bpm, index, note, velocity, timing_bpm, clock_bpm;
    enum ArpDivision division;
    bool playing, recording, captured, seek_attack;
} StepSequencer;

typedef struct {
    uint8_t matrix[MATRIX_SIZE][MATRIX_SIZE];
    uint8_t ks_routes[MATRIX_SIZE];
    uint8_t usb_routes[2][MATRIX_SIZE];
    Route routes[MATRIX_SIZE][ROUTE_SOURCES];
    uint8_t counts[MATRIX_SIZE];
    uint8_t velocity[MIDI_NOTES];
    uint8_t held[MIDI_NOTES];
    size_t held_count;
    unsigned pitch_note;
    unsigned midi_channel;   /* 0 = todos os canais, 1–16 = canal específico */
    uint32_t noise;
    ZiviReal rate, tune, shape, level, mix, lfo, cutoff, bend, stick_x, stick_y;
    ZiviReal phase, osc2_phase, lfo_phase;
    bool live_attack, threshold_active, ks_matrix;
    ZiviReal threshold_probe;
    Ks ks;
    Arp arp;
    StepSequencer sequencer;
    unsigned clock_bpm;
    Modules modules;
    ZiviReal signals[ROUTE_SOURCES];
    ZiviReal meter_peak;
    ZiviReal hz[4]; /* Osciladores 1–3 e corte efetivo na amostra mais recente. */
} Synth;

bool synth_init(Synth *synth, ZiviReal rate);
void synth_safe_patch(Synth *synth);
void synth_ks_patch(Synth *synth);
void synth_spring_patch(Synth *synth);
void synth_repeat_patch(Synth *synth);
unsigned synth_scale_note(unsigned note, unsigned root, enum ScaleMode mode);
void synth_arp(Synth *synth, bool enabled, unsigned bpm, unsigned gate, enum ArpOrder order, enum ArpDivision division);
void synth_arp_scale(Synth *synth, unsigned root, enum ScaleMode mode);
void synth_arp_modes(Synth *synth, bool hold, bool free_run, unsigned chance);
void synth_seq_timing(Synth *synth, unsigned length, unsigned bpm);
void synth_seq_play(Synth *synth, bool playing);
void synth_seq_seek(Synth *synth, uint32_t frames);
void synth_sync_tempo(Synth *synth, unsigned bpm);
void synth_seq_record(Synth *synth);
bool synth_seq_from_ks(Synth *synth);
bool synth_seq_to_ks(Synth *synth);
bool synth_seq_set(Synth *synth, unsigned index, unsigned note, unsigned velocity);
void synth_ks_record(Synth *synth);
void synth_ks_random(Synth *synth, bool down);
void synth_clear_matrix(Synth *synth);
const char *synth_source_name(unsigned row);
const char *synth_dest_name(unsigned col);
bool synth_pin_supported(unsigned row, unsigned col);
bool synth_pin_delayed(unsigned row, unsigned col);
bool synth_usb_route(Synth *synth, unsigned channel, unsigned col, enum Pin amount);
bool synth_ks_route(Synth *synth, unsigned col, enum Pin amount);
void synth_ks_matrix(Synth *synth, bool enabled);
bool synth_pin(Synth *synth, unsigned row, unsigned col, enum Pin pin);
bool synth_toggle_pin(Synth *synth, unsigned row, unsigned col, enum Pin pin);
void synth_controls(Synth *synth, ZiviReal tune, ZiviReal shape, ZiviReal level);
void synth_patch_controls(Synth *synth, ZiviReal mix, ZiviReal lfo, ZiviReal cutoff);
void synth_module_control(Synth *synth, unsigned param, ZiviReal value);
void synth_manual_attack(Synth *synth);
void synth_manual_hold(Synth *synth, bool down);
void synth_stick_x(Synth *synth, ZiviReal value);
void synth_stick_y(Synth *synth, ZiviReal value);
void synth_all_notes_off(Synth *synth);
void synth_midi_channel(Synth *synth, unsigned channel);
bool synth_midi(Synth *synth, unsigned status, unsigned data1, unsigned data2);
void synth_render(Synth *synth, float *mono, size_t frames);
void synth_render_stereo(Synth *synth, float *left, float *right, size_t frames);
/* NULL preserva o sinal de controle KS no respectivo canal. */
void synth_render_usb(Synth *synth, float *left, float *right, size_t frames,
                      const float *input1, const float *input2);
void synth_render_input(Synth *synth, float *left, float *right, size_t frames,
                        const float *input1, const float *input2);

#endif
