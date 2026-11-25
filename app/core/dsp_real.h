#ifndef ZIVI_DSP_REAL_H
#define ZIVI_DSP_REAL_H

#include <math.h>

#ifdef ZIVI_REAL32
typedef float ZiviReal;

/* The Pro runs the sample loop in the native floating-point width.
 * The tables and interpolation cover the bounded routing ranges. */
#define dsp_exp expf
static inline ZiviReal dsp_abs(ZiviReal x) { return __builtin_fabsf(x); }
static inline ZiviReal dsp_max(ZiviReal a, ZiviReal b) { return a > b ? a : b; }
static inline ZiviReal dsp_min(ZiviReal a, ZiviReal b) { return a < b ? a : b; }
#else
typedef double ZiviReal;
#define dsp_exp exp
#define dsp_abs fabs
#define dsp_max fmax
#define dsp_min fmin
#endif

#endif
