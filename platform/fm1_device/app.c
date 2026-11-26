#include "app.h"
#include "../../app/core/presets.h"
#include <stdio.h>

#define ZIVI_BSP_LIBC

static uint32_t zivi_display_bits(float value)
{
    union { float value; uint32_t bits; } encoded = {value};
    return encoded.bits;
}

#define ZIVI_LCD_FLOAT_BITS
#define ZIVI_LABEL(value) zivi_display_bits(value)
#include "../smk37_pro/format.h"

#define snprintf lcd_format
#define exp expf
#define exp2 exp2f
#define fmax fmaxf
#define fmin fminf
#define fabs fabsf
#define lround lroundf
#define sin sinf
#define cos cosf
#define log logf
#define sqrt sqrtf

#include "build/fm1-panel.inc"
