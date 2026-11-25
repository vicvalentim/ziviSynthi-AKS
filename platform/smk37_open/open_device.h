#ifndef ZIVI_OPEN_DEVICE_H
#define ZIVI_OPEN_DEVICE_H

#include "fm1.h"

FM1_AUDIO_CODE int zivi_open_audio(int32_t *output, unsigned frames);
void zivi_open_midi(unsigned source, const uint8_t *message, size_t bytes);

#endif
