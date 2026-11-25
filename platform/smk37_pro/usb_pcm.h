#ifndef ZIVI_USB_PCM_H
#define ZIVI_USB_PCM_H

#include <stdbool.h>
#include <stdint.h>

enum { USB_PCM_CAPACITY = 1024, USB_PCM_PACKET = 184, USB_PCM_BLOCK = 128 };

typedef struct {
    uint32_t samples[USB_PCM_CAPACITY];
    unsigned read, write, underruns, overruns;
    bool primed;
} ZiviUsbPcm;

static inline void pcm_reset(ZiviUsbPcm *pcm)
{
    pcm->read = pcm->write = 0;
    pcm->primed = false;
}

static inline void pcm_receive(ZiviUsbPcm *pcm, const uint8_t *usb, unsigned bytes)
{
    if (bytes > USB_PCM_PACKET || (bytes & 3u)) return;
    unsigned frames = bytes / 4;
    if (frames > USB_PCM_CAPACITY - (pcm->write - pcm->read)) {
        ++pcm->overruns;
        return;
    }
    unsigned write = pcm->write;
    for (unsigned i = 0; i < frames; ++i, usb += 4) {
        pcm->samples[write++ % USB_PCM_CAPACITY] =
            (uint32_t)usb[0] | (uint32_t)usb[1] << 8 | (uint32_t)usb[2] << 16 | (uint32_t)usb[3] << 24;
    }
    pcm->write = write;
}

static inline int32_t pcm_clip(int32_t sample)
{
    return sample > 8388607 ? 8388607 : sample < -8388608 ? -8388608 : sample;
}

static inline int32_t pcm_interp(int16_t a, int16_t b, unsigned fraction)
{
    return (int32_t)a * (int32_t)(USB_PCM_BLOCK - fraction) + (int32_t)b * (int32_t)fraction;
}

/* Stereo 16-bit USB into the analog Q23 mix with gain from 0 to 128. Adaptive consumption
 * corrects the small drift between the USB and the codec clocks. */
static inline void pcm_mix(ZiviUsbPcm *pcm, int32_t *out, unsigned gain)
{
    unsigned fill = pcm->write - pcm->read;
    if (fill < USB_PCM_BLOCK + 2) {
        if (pcm->primed) ++pcm->underruns;
        pcm->primed = false;
        return;
    }
    if (!pcm->primed && fill < 2 * USB_PCM_BLOCK) return;
    pcm->primed = true;
    unsigned step = fill > 384 ? 129 : fill < 192 ? 127 : 128;
    unsigned position = 0, read = pcm->read;
    for (unsigned i = 0; i < USB_PCM_BLOCK; ++i, position += step) {
        unsigned index = read + position / USB_PCM_BLOCK;
        unsigned fraction = position % USB_PCM_BLOCK;
        uint32_t a = pcm->samples[index % USB_PCM_CAPACITY];
        uint32_t b = pcm->samples[(index + 1) % USB_PCM_CAPACITY];
        out[2 * i] = pcm_clip(out[2 * i] + (pcm_interp((int16_t)a, (int16_t)b, fraction) * (int32_t)gain) / 64);
        out[2 * i + 1] = pcm_clip(out[2 * i + 1] + (pcm_interp((int16_t)(a >> 16), (int16_t)(b >> 16), fraction) * (int32_t)gain) / 64);
    }
    pcm->read = read + step;
}

#endif
