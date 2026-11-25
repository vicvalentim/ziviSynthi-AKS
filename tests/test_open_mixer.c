#include "fm1_audio_io.h"
#include "fm1_engine_control.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

enum { CEILING = 7476354 };
static int playback;
static bool bluetooth;
static int32_t usb_signal = 1000000;
static uint16_t usb_level = 128;
static int16_t bt_signal = 1000;
static int32_t captured[FM1_BLOCK_WORDS], output[FM1_BLOCK_WORDS];

unsigned fm1_engine_output_level(void) { return 127; }
int fm1_usb_audio_read(int32_t *pcm, unsigned frames, uint16_t gains_q7[2])
{
    assert(frames == FM1_BLOCK_FRAMES);
    for (unsigned i = 0; i < FM1_BLOCK_WORDS; ++i) pcm[i] = usb_signal;
    gains_q7[0] = gains_q7[1] = usb_level;
    return playback;
}
int fm1_usb_audio_write(const int32_t *pcm, unsigned frames)
{
    assert(frames == FM1_BLOCK_FRAMES);
    for (unsigned i = 0; i < FM1_BLOCK_WORDS; ++i) captured[i] = pcm[i];
    return (int)frames;
}
int fm1_bluetooth_audio_read(int16_t *pcm, unsigned frames)
{
    assert(frames == FM1_BLOCK_FRAMES);
    if (!bluetooth) return 0;
    for (unsigned i = 0; i < FM1_BLOCK_WORDS; ++i) pcm[i] = bt_signal;
    return (int)frames;
}
void fm1_output_mix(int32_t *output);
void fm1_output_reset(void);

static void mix(int32_t left, int32_t right)
{
    for (unsigned i = 0; i < FM1_BLOCK_WORDS; ++i) output[i] = i & 1U ? right : left;
    fm1_output_mix(output);
    for (unsigned i = 0; i < FM1_BLOCK_WORDS; ++i) {
        assert(output[i] == captured[i]);
        assert(output[i] >= -CEILING && output[i] <= CEILING);
    }
}

static int32_t steady_mix(int32_t signal)
{
    mix(signal, signal);
    mix(signal, signal);
    return output[0];
}

int main(void)
{
    fm1_output_reset();
    for (unsigned i = 0; i < 16; ++i) mix(0, 0);
    int32_t reference = steady_mix(1000000);
    assert(reference == (1000000 * 127) / 128);
    playback = -2;
    assert(steady_mix(1000000) == reference);
    bluetooth = true;
    assert(steady_mix(1000000) == reference + 254000);
    playback = FM1_BLOCK_FRAMES;
    assert(steady_mix(1000000) == 2 * reference + 254000);
    /* Two inputs exceed full scale and the last offsets them before limiting. */
    usb_signal = 7500000;
    bt_signal = -30000;
    assert(steady_mix(7500000) == 2 * ((7500000 * 127) / 128) - 30000 * 254);
    puts("Mixer Open: reinício USB preserva síntese e Bluetooth no codec e na captura.");

    playback = 0;
    bluetooth = false;
    mix(1000000, -1000000);
    mix(18000000, -9000000);
    /* Preceding block ramps down before the first overloaded stereo frame. */
    assert(output[0] == reference && output[FM1_BLOCK_WORDS - 2] < reference / 2);
    mix(12000000, -24000000);
    assert(output[0] > 7400000 && abs(output[0] + 2 * output[1]) < 4);
    assert(output[FM1_BLOCK_WORDS - 2] < output[0]);
    mix(1000000, 1000000);
    assert(output[1] < -7400000 && abs(2 * output[0] + output[1]) < 4);
    mix(1000000, 1000000);
    assert(output[0] < reference / 2 && output[FM1_BLOCK_WORDS - 2] > output[0]);
    for (unsigned i = 0; i < 128; ++i) mix(1000000, 1000000);
    assert(output[0] == reference);

    /* Full-scale USB and Bluetooth overload reach the same limiter. */
    usb_signal = 8388607;
    bt_signal = 32767;
    usb_level = 128;
    playback = FM1_BLOCK_FRAMES;
    bluetooth = true;
    int32_t peak = steady_mix(8388607);
    assert(peak > 7400000 && peak < CEILING);
    usb_level = UINT16_MAX;
    usb_signal = -8388608;
    assert(steady_mix(-33554430) < -7100000);
    fm1_output_reset();
    mix(0, 0);
    assert(output[0] == 0 && output[FM1_BLOCK_WORDS - 1] == 0);
    puts("Limiter master: teto -1 dBFS, ganho estéreo vinculado, antecipação, recuperação e soma de três fontes aprovados.");
}
