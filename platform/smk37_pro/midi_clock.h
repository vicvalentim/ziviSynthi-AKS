#ifndef ZIVI_MIDI_CLOCK_H
#define ZIVI_MIDI_CLOCK_H

#include "ks.h"

typedef struct {
    uint32_t last_tick, period_q8, resume_frame;
    bool external, pending_start, seen, locked;
} SmkMidiClock;

/* A duplicate USB/BLE timestamp does not advance the tempo estimator. */
static inline bool midi_clock_measure(SmkMidiClock *clock, uint32_t ms)
{
    if (clock->seen) {
        uint32_t interval = ms - clock->last_tick;
        if (interval < 5) return false;
        if (interval <= 125) {
            uint32_t measured = interval * 256u;
            clock->period_q8 = clock->locked ? (clock->period_q8 * 7 + measured + 4) / 8 : measured;
            clock->locked = true;
        }
    }
    clock->last_tick = ms;
    clock->seen = true;
    return true;
}

static inline void midi_clock_input(SmkMidiClock *clock, Ks *ks, unsigned status,
                                    unsigned lo, unsigned hi, uint32_t ms)
{
    if (!clock->external) return;
    switch (status) {
    case 0xf8:
        if (!midi_clock_measure(clock, ms)) break;
        if (clock->locked) {
            unsigned speed = (125u * 256u * 100u + clock->period_q8 * 3u) / (clock->period_q8 * 6u);
            ks->speed = speed < 25 ? 25 : speed > 400 ? 400 : speed;
        }
        if (clock->pending_start) {
            clock->pending_start = false;
            ks_play_from(ks, clock->resume_frame);
        }
        break;
    case 0xfa:
        clock->resume_frame = 0;
        clock->pending_start = true;
        break;
    case 0xfb:
        clock->pending_start = true;
        break;
    case 0xfc:
        clock->pending_start = false;
        if (ks->mode == KS_PLAYING) {
            clock->resume_frame = ks->position / 100u;
            ks_stop(ks);
        }
        break;
    case 0xf2: {
        /* MIDI SPP uses sixteenth notes. Factory phrases use 120 BPM as
         * the nominal reference for the external KS transport. */
        unsigned step = lo | (hi << 7);
        clock->resume_frame = (step * ks->rate + 4u) / 8u;
        if (ks->mode == KS_PLAYING) ks_play_from(ks, clock->resume_frame);
        break;
    }
    }
}

static inline void midi_clock_timeout(SmkMidiClock *clock, Ks *ks, uint32_t ms)
{
    if (clock->external && clock->seen && ms - clock->last_tick > 500) {
        midi_clock_input(clock, ks, 0xfc, 0, 0, ms);
        clock->seen = clock->locked = false;
    }
}

#endif
