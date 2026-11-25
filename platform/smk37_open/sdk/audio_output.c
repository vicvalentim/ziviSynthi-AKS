/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "audio_output.h"
#include "fm1_audio_io.h"
#include "fm1_engine_control.h"

enum { UNITY = 4096, CEILING_Q23 = 7476354 }; /* -1 dBFS sample peak. */
_Static_assert(FM1_BLOCK_FRAMES == 128, "Limiter ramp requires 128 frames");
static unsigned output_gain;
static int32_t usb_pcm[FM1_BLOCK_WORDS] __attribute__((aligned(32)));
static int16_t bluetooth_pcm[FM1_BLOCK_WORDS] __attribute__((aligned(32)));
static int32_t delayed_pcm[FM1_BLOCK_WORDS] __attribute__((aligned(32)));
static int limiter_gain, delayed_limit;

void fm1_output_reset(void)
{
    output_gain = 0;
    limiter_gain = delayed_limit = UNITY;
    for (unsigned i = 0; i < FM1_BLOCK_WORDS; ++i) delayed_pcm[i] = 0;
}

FM1_AUDIO_CODE void fm1_output_mix(int32_t *output)
{
    if (!output) return;
    unsigned target = fm1_engine_output_level();
    if (target > 127U) target = 127U;
    int delta = (int)target - (int)output_gain;
    output_gain = (unsigned)((int)output_gain + (delta > 8 ? 8 : delta < -8 ? -8 : delta));

    uint16_t usb_gain[2] = {0, 0};
    int usb_frames = fm1_usb_audio_read(usb_pcm, FM1_BLOCK_FRAMES, usb_gain);
    unsigned left = ((unsigned)usb_gain[0] * output_gain) >> 7;
    unsigned right = ((unsigned)usb_gain[1] * output_gain) >> 7;
    int bt_frames = fm1_bluetooth_audio_read(bluetooth_pcm, FM1_BLOCK_FRAMES);
    int bt_gain = bt_frames == FM1_BLOCK_FRAMES ? (int)(output_gain * 2U) : 0;
    unsigned peak = 0;
    for (unsigned i = 0; i < FM1_BLOCK_WORDS; ++i) {
        int64_t value = ((int64_t)output[i] * output_gain) >> 7;
        if (usb_frames == FM1_BLOCK_FRAMES)
            value += ((int64_t)usb_pcm[i] * ((i & 1U) ? right : left)) >> 7;
        if (bt_gain) value += (int32_t)bluetooth_pcm[i] * bt_gain;
        /* Preserve mix headroom and bound the conversion even at maximum host gain. */
        int32_t sample = value > INT32_MAX ? INT32_MAX : value < -INT32_MAX ? -INT32_MAX : (int32_t)value;
        output[i] = sample;
        unsigned magnitude = (unsigned)(sample < 0 ? -sample : sample);
        if (magnitude > peak) peak = magnitude;
    }

    int next_limit = UNITY;
    if (peak > CEILING_Q23)
        next_limit = CEILING_Q23 / (int)(((peak - 1U) >> 12) + 1U);
    /* Start gain was bounded by this block's peak on the previous call.
     * End gain obeys both blocks' limits. Ease back over about 45 ms. */
    int end_gain = limiter_gain + (UNITY - limiter_gain + 15) / 16;
    if (end_gain > delayed_limit) end_gain = delayed_limit;
    if (end_gain > next_limit) end_gain = next_limit;
    int ramp = limiter_gain * FM1_BLOCK_FRAMES;
    int step = end_gain - limiter_gain;
    for (unsigned i = 0; i < FM1_BLOCK_WORDS; i += 2) {
        int gain = ramp >> 7;
        for (unsigned ch = 0; ch < 2; ++ch) {
            unsigned word = i + ch;
            int32_t previous = delayed_pcm[word];
            delayed_pcm[word] = output[word];
            output[word] = (int32_t)(((int64_t)previous * gain) >> 12);
        }
        ramp += step;
    }
    limiter_gain = end_gain;
    delayed_limit = next_limit;
    (void)fm1_usb_audio_write(output, FM1_BLOCK_FRAMES);
}
