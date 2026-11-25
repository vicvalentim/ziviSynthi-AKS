#ifndef ZIVI_FELUCCA_MASTER_H
#define ZIVI_FELUCCA_MASTER_H

#include <stdint.h>

enum { MASTER_FRAMES = 128, UNITY_Q12 = 4096, CEILING_Q23 = 7476354 };
typedef struct {
    int32_t delay[2 * MASTER_FRAMES];
    int gain, limit;
} MasterLimiter;

static inline int32_t q12_scale(int32_t sample, int gain)
{
    /* Split the signed sample so the PI32v2 needs no paired multiply. */
    return (sample >> 12) * gain + ((sample & 4095) * gain >> 12);
}

static inline void master_limit_block(MasterLimiter *state, int32_t *pcm)
{
    unsigned peak = 0;
    for (unsigned i = 0; i < 2 * MASTER_FRAMES; ++i) {
        unsigned sample = (unsigned)__builtin_abs(pcm[i]);
        if (sample > peak) peak = sample;
    }
    int next_limit = UNITY_Q12;
    if (peak > CEILING_Q23) next_limit = CEILING_Q23 / (int)((peak + 4095) >> 12);
    /* The previous ramp already bounded this block's start gain. */
    int end_gain = state->gain + (UNITY_Q12 - state->gain + 15) / 16;
    if (end_gain > state->limit) end_gain = state->limit;
    if (end_gain > next_limit) end_gain = next_limit;
    int ramp = state->gain * MASTER_FRAMES, step = end_gain - state->gain;
    for (unsigned frame = 0; frame < MASTER_FRAMES; ++frame) {
        int gain = ramp >> 7;
        for (unsigned channel = 0; channel < 2; ++channel) {
            unsigned index = 2 * frame + channel;
            int32_t delayed = state->delay[index];
            state->delay[index] = pcm[index];
            pcm[index] = q12_scale(delayed, gain);
        }
        ramp += step;
    }
    state->gain = end_gain;
    state->limit = next_limit;
}

#endif
