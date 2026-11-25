#ifndef ZIVI_DSP_CURVE_H
#define ZIVI_DSP_CURVE_H

#include "dsp_real.h"

#ifdef ZIVI_REAL32
#include <stdint.h>
#include "../../build/smk37-dsp-tables.inc"

static inline ZiviReal dsp_curve(const float *table, float index)
{
    unsigned low = (unsigned)index;
    return table[low] + (table[low + 1] - table[low]) * (index - low);
}

static inline ZiviReal dsp_exp2(ZiviReal value)
{
    int power = (int)value;
    if (power > value) --power;
    union { uint32_t bits; float value; } scale = { (uint32_t)(power + 127) << 23 };
    return scale.value * dsp_curve(ZIVI_EXP2_TABLE, DSP_TABLE_SIZE * (value - power));
}

static inline ZiviReal dsp_sine(ZiviReal phase)
{
    return dsp_curve(ZIVI_SINE_TABLE, DSP_TABLE_SIZE * phase);
}

static inline ZiviReal dsp_filter_g(ZiviReal normalized_cutoff)
{
    return dsp_curve(ZIVI_FILTER_TABLE, DSP_TABLE_SIZE * normalized_cutoff * (1.0f / 0.22f));
}

static inline ZiviReal dsp_decay_coeff(ZiviReal frames)
{
    /* 3 ms minimum, at 44.1 kHz. Avoid subtracting numbers close to one. */
    float x = 1.0f / frames;
    return x * (1.0f - x * (0.5f - x * (1.0f / 6.0f)));
}
#else
#define dsp_exp2 exp2

static inline ZiviReal dsp_sine(ZiviReal phase)
{
    return sin(6.28318530717958647692 * phase);
}

static inline ZiviReal dsp_filter_g(ZiviReal normalized_cutoff)
{
    double g = tan(3.14159265358979323846 * normalized_cutoff);
    return g / (1.0 + g);
}

static inline ZiviReal dsp_decay_coeff(ZiviReal frames)
{
    return 1.0 - exp(-1.0 / frames);
}
#endif

#endif
