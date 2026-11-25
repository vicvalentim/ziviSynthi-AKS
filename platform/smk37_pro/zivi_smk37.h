#ifndef ZIVI_SMK37_H
#define ZIVI_SMK37_H

#include "presets.h"

#ifndef ZIVI_SMK_AUDIO_SLOTS
#define ZIVI_SMK_AUDIO_SLOTS 3
#endif
#ifndef ZIVI_SMK_RENDER_RATE
#define ZIVI_SMK_RENDER_RATE 44100
#endif
#ifndef ZIVI_SMK_MIDI_PORTS
#define ZIVI_SMK_MIDI_PORTS 1
#endif

enum {
    ZIVI_SMK_RATE = ZIVI_SMK_RENDER_RATE,
    ZIVI_SMK_FRAMES = 128,
    ZIVI_SMK_WORDS = 2 * ZIVI_SMK_FRAMES,
    ZIVI_SMK_KEYS = 37,
    ZIVI_SMK_SLOTS = ZIVI_SMK_AUDIO_SLOTS
};

typedef struct {
    Synth synth;
    /* Single-core SPSC ring. Producer owns write; only the audio IRQ owns read. */
    volatile unsigned read, write, underruns;
    int32_t ring[ZIVI_SMK_SLOTS][ZIVI_SMK_WORDS];
    uint8_t key_note[ZIVI_SMK_KEYS];
    struct {
        uint16_t down[128], sustained[128], sustain;
    } midi[ZIVI_SMK_MIDI_PORTS];
    unsigned gain_q12;
    int32_t previous_tail[2];
    unsigned last_render_underruns;
    bool muted, patch_transition;
#ifdef ZIVI_SMK_OPEN_AUDIO
    int32_t dc[2]; /* Producer-owned Q28 DC estimates. */
    int32_t played_tail[2];
    bool recovering; /* Consumer-owned underrun envelope state. */
#endif
} ZiviSmk37;

void zivi_smk_init(ZiviSmk37 *engine);
bool zivi_smk_key_note(ZiviSmk37 *engine, unsigned key, unsigned velocity, unsigned note);
bool zivi_smk_key(ZiviSmk37 *engine, unsigned key, unsigned velocity, int octave);
bool zivi_smk_midi(ZiviSmk37 *engine, unsigned status, unsigned data1, unsigned data2);
bool zivi_smk_midi_port(ZiviSmk37 *engine, unsigned port, unsigned status, unsigned data1, unsigned data2);
void zivi_smk_midi_port_disconnect(ZiviSmk37 *engine, unsigned port);
void zivi_smk_midi_disconnect(ZiviSmk37 *engine);
void zivi_smk_panic(ZiviSmk37 *engine);
bool zivi_smk_pad(ZiviSmk37 *engine, unsigned row, unsigned col, unsigned pad, enum Pin kind);
bool zivi_smk_fill(ZiviSmk37 *engine);
bool zivi_smk_fill_input(ZiviSmk37 *engine, const float *input1, const float *input2);
bool zivi_smk_irq_read(ZiviSmk37 *engine, volatile int32_t *words);

#endif
