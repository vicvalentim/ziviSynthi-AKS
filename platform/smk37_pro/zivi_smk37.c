#include "zivi_smk37.h"

#include <string.h>

#if defined(ZIVI_SMK_CHECK_SDK)
#include "fm1_block.h"
#ifdef ZIVI_SMK_OPEN_AUDIO
#include "fm1_audio.h"
_Static_assert(ZIVI_SMK_RATE == FM1_AUDIO_HARDWARE_RATE, "SDK codec sample rate mismatch");
#else
_Static_assert(ZIVI_SMK_RATE == FM1_SAMPLE_RATE, "SDK sample rate mismatch");
#endif
_Static_assert(ZIVI_SMK_FRAMES == FM1_BLOCK_FRAMES, "SDK block size mismatch");
_Static_assert(ZIVI_SMK_WORDS == FM1_BLOCK_WORDS, "SDK stereo layout mismatch");
#endif

#if defined(ZIVI_SMK_NATIVE)
#define DMA_CODE __attribute__((section(".ram_text.zivi_audio"), noinline))
static inline __attribute__((always_inline)) void buffer_fence(void)
{
    __asm__ volatile("csync" ::: "memory");
}
#else
#include <stdatomic.h>
#define DMA_CODE
static inline void buffer_fence(void)
{
    atomic_signal_fence(memory_order_seq_cst);
}
#endif

void zivi_smk_init(ZiviSmk37 *engine)
{
    synth_init(&engine->synth, ZIVI_SMK_RATE);
    memset(engine->key_note, 255, sizeof engine->key_note);
    memset(engine->midi, 0, sizeof engine->midi);
    engine->read = engine->write = engine->underruns = 0;
    engine->gain_q12 = 4096;
    engine->muted = engine->patch_transition = false;
    engine->previous_tail[0] = engine->previous_tail[1] = 0;
    engine->last_render_underruns = 0;
#ifdef ZIVI_SMK_OPEN_AUDIO
    engine->dc[0] = engine->dc[1] = 0;
    engine->played_tail[0] = engine->played_tail[1] = 0;
    engine->recovering = false;
#endif
}

static bool local_note(const ZiviSmk37 *engine, unsigned note)
{
    for (unsigned i = 0; i < ZIVI_SMK_KEYS; ++i)
        if (engine->key_note[i] == note) return true;
    return false;
}

static void release_note(ZiviSmk37 *engine, unsigned note)
{
    if (note >= 128) return;
    for (unsigned port = 0; port < ZIVI_SMK_MIDI_PORTS; ++port)
        if (engine->midi[port].down[note] | engine->midi[port].sustained[note]) return;
    if (local_note(engine, note)) return;
    unsigned channel = engine->synth.midi_channel ? engine->synth.midi_channel - 1 : 0;
    synth_midi(&engine->synth, 0x80u | channel, note, 0);
}

bool zivi_smk_key_note(ZiviSmk37 *engine, unsigned key, unsigned velocity, unsigned note)
{
    if (key >= ZIVI_SMK_KEYS || velocity > 127 || (velocity && note >= MIDI_NOTES)) return false;
    unsigned previous = engine->key_note[key];
    if (!velocity) {
        engine->key_note[key] = 255;
        release_note(engine, previous);
        return true;
    }
    engine->key_note[key] = (uint8_t)note;
    if (previous != note) release_note(engine, previous);
    unsigned channel = engine->synth.midi_channel ? engine->synth.midi_channel - 1 : 0;
    return synth_midi(&engine->synth, 0x90u | channel, note, velocity);
}

bool zivi_smk_key(ZiviSmk37 *engine, unsigned key, unsigned velocity, int octave)
{
    if (key >= ZIVI_SMK_KEYS || octave < -4 || octave > 3) return false;
    return zivi_smk_key_note(engine, key, velocity, (unsigned)(48 + (int)key + octave * 12));
}

static void release_midi_channel(ZiviSmk37 *engine, unsigned port, uint16_t mask, bool sustained_only)
{
    engine->midi[port].sustain &= (uint16_t)~mask;
    for (unsigned note = 0; note < 128; ++note) {
        unsigned previous = engine->midi[port].sustained[note];
        engine->midi[port].sustained[note] &= (uint16_t)~mask;
        if (!sustained_only) {
            previous |= engine->midi[port].down[note];
            engine->midi[port].down[note] &= (uint16_t)~mask;
        }
        if (previous & mask) release_note(engine, note);
    }
}

bool zivi_smk_midi_port(ZiviSmk37 *engine, unsigned port, unsigned status, unsigned data1, unsigned data2)
{
    if (port >= ZIVI_SMK_MIDI_PORTS || status < 0x80 || status > 0xef || data1 >= 128 || data2 >= 128) return false;
    unsigned channel = (status & 15) + 1;
    if (engine->synth.midi_channel && engine->synth.midi_channel != channel) return false;
    uint16_t mask = (uint16_t)(1u << (channel - 1));
    switch (status & 0xf0) {
    case 0x90:
        if (data2) {
            engine->midi[port].down[data1] |= mask;
            engine->midi[port].sustained[data1] &= (uint16_t)~mask;
            return synth_midi(&engine->synth, status, data1, data2);
        }
        /* FALLTHROUGH */
    case 0x80:
        engine->midi[port].sustained[data1] |= engine->midi[port].down[data1] & engine->midi[port].sustain & mask;
        engine->midi[port].down[data1] &= (uint16_t)~mask;
        release_note(engine, data1);
        return true;
    case 0xb0:
        if (data1 == 64) {
            if (data2 >= 64) engine->midi[port].sustain |= mask;
            else release_midi_channel(engine, port, mask, true);
            return true;
        }
        if (data1 == 120 || data1 == 123) {
            release_midi_channel(engine, port, mask, false);
            return true;
        }
        break;
    }
    return synth_midi(&engine->synth, status, data1, data2);
}

bool zivi_smk_midi(ZiviSmk37 *engine, unsigned status, unsigned data1, unsigned data2)
{
    return zivi_smk_midi_port(engine, 0, status, data1, data2);
}

void zivi_smk_midi_port_disconnect(ZiviSmk37 *engine, unsigned port)
{
    if (port < ZIVI_SMK_MIDI_PORTS) release_midi_channel(engine, port, UINT16_MAX, false);
}

void zivi_smk_midi_disconnect(ZiviSmk37 *engine)
{
    for (unsigned port = 0; port < ZIVI_SMK_MIDI_PORTS; ++port)
        zivi_smk_midi_port_disconnect(engine, port);
}

void zivi_smk_panic(ZiviSmk37 *engine)
{
    memset(engine->key_note, 255, sizeof engine->key_note);
    memset(engine->midi, 0, sizeof engine->midi);
    engine->patch_transition = false;
    engine->previous_tail[0] = engine->previous_tail[1] = 0;
#ifdef ZIVI_SMK_OPEN_AUDIO
    engine->dc[0] = engine->dc[1] = 0;
#endif
    synth_all_notes_off(&engine->synth);
    synth_manual_hold(&engine->synth, false);
    engine->synth.modules.manual_attack = false;
    ks_stop(&engine->synth.ks);
    engine->synth.modules.env = 0;
    engine->synth.modules.env_running = false;
    engine->synth.modules.phase = ENV_OFF;
    engine->synth.modules.manual_attack = false;
}

bool zivi_smk_pad(ZiviSmk37 *engine, unsigned row, unsigned col, unsigned pad, enum Pin kind)
{
    if (row >= MATRIX_SIZE || col >= MATRIX_SIZE || pad >= MATRIX_SIZE) return false;
    unsigned tile_row = (row / MATRIX_TILE) * MATRIX_TILE;
    unsigned tile_col = (col / MATRIX_TILE) * MATRIX_TILE;
    return synth_toggle_pin(&engine->synth, tile_row + pad / MATRIX_TILE, tile_col + pad % MATRIX_TILE, kind);
}

static int32_t pcm_q23(float sample)
{
    /* Bound the conversion before casting, including NaN. */
#ifdef ZIVI_SMK_OPEN_AUDIO
    /* Extra headroom for the master limiter that follows the mixer. Keep
     * 32x sample, the DC difference and the 32-frame fades in int32_t. */
    enum { LOW = -33554430, HIGH = 33554430 };
#else
    enum { LOW = -8388608, HIGH = 8388607 };
#endif
    if (!(sample >= (float)LOW)) return sample < 0.0f ? LOW : 0;
    if (sample >= (float)HIGH) return HIGH;
    return (int32_t)sample;
}

#ifdef ZIVI_SMK_OPEN_AUDIO
static int32_t dc_block(int32_t *estimate, int32_t sample)
{
    /* 1/2048 pole step at the codec rate: about 3.43 Hz. Q28 keeps
     * sub-Q23 resolution and the difference fits signed 32 bits. */
    *estimate += (sample * 32 - *estimate) / 2048;
    return sample - *estimate / 32;
}
#endif

bool zivi_smk_fill(ZiviSmk37 *engine)
{
    return zivi_smk_fill_input(engine, NULL, NULL);
}

bool zivi_smk_fill_input(ZiviSmk37 *engine, const float *input1, const float *input2)
{
    unsigned at = engine->write;
    unsigned next = at + 1 == ZIVI_SMK_SLOTS ? 0 : at + 1;
    if (next == engine->read) return false;

    float left[ZIVI_SMK_FRAMES], right[ZIVI_SMK_FRAMES];
    if (engine->muted) {
        memset(engine->ring[at], 0, sizeof engine->ring[at]);
    } else {
        float gain = (float)engine->gain_q12 * (8388607.0f / 4096.0f);
        synth_render_usb(&engine->synth, left, right, ZIVI_SMK_FRAMES, input1, input2);
        for (unsigned i = 0; i < ZIVI_SMK_FRAMES; ++i) {
            int32_t l = pcm_q23(left[i] * gain), r = pcm_q23(right[i] * gain);
#ifdef ZIVI_SMK_OPEN_AUDIO
            l = dc_block(&engine->dc[0], l);
            r = dc_block(&engine->dc[1], r);
#endif
            engine->ring[at][2 * i] = l;
            engine->ring[at][2 * i + 1] = r;
        }
    }
    unsigned underruns = engine->underruns;
    int32_t *pcm = engine->ring[at];
    if (engine->patch_transition && !engine->muted) {
        /* Smooth the transition in the producer. The first frame matches the
         * queued tail, then the new patch takes over within 32 frames. */
        enum { RAMP = 32 };
        for (unsigned channel = 0; channel < 2; ++channel) {
            int32_t tail = underruns == engine->last_render_underruns ? engine->previous_tail[channel] : 0;
            for (int i = 0; i < RAMP; ++i) {
                unsigned word = 2 * i + channel;
                pcm[word] = (tail * (RAMP - i) + pcm[word] * i) / RAMP;
            }
        }
    }
    engine->patch_transition = false;
    engine->previous_tail[0] = pcm[ZIVI_SMK_WORDS - 2];
    engine->previous_tail[1] = pcm[ZIVI_SMK_WORDS - 1];
    engine->last_render_underruns = underruns;
    buffer_fence();
    engine->write = next;
    return true;
}

DMA_CODE bool zivi_smk_irq_read(ZiviSmk37 *engine, volatile int32_t *words)
{
    unsigned at = engine->read;
    if (at == engine->write) {
        ++engine->underruns;
        unsigned i = 0;
#ifdef ZIVI_SMK_OPEN_AUDIO
        if (!engine->recovering) {
            for (; i < 32; ++i) {
                words[2 * i] = engine->played_tail[0] * (int32_t)(32 - i) / 32;
                words[2 * i + 1] = engine->played_tail[1] * (int32_t)(32 - i) / 32;
            }
            engine->recovering = true;
        }
        i *= 2;
#endif
        for (; i < ZIVI_SMK_WORDS; ++i) words[i] = 0;
        return false;
    }
    buffer_fence();
    const int32_t *pcm = engine->ring[at];
    unsigned i = 0;
#ifdef ZIVI_SMK_OPEN_AUDIO
    if (engine->recovering) {
        for (; i < 32; ++i) {
            words[2 * i] = pcm[2 * i] * (int32_t)i / 32;
            words[2 * i + 1] = pcm[2 * i + 1] * (int32_t)i / 32;
        }
        i *= 2;
        engine->recovering = false;
    }
    engine->played_tail[0] = pcm[ZIVI_SMK_WORDS - 2];
    engine->played_tail[1] = pcm[ZIVI_SMK_WORDS - 1];
#endif
    for (; i < ZIVI_SMK_WORDS; ++i) words[i] = pcm[i];
    buffer_fence();
    engine->read = at + 1 == ZIVI_SMK_SLOTS ? 0 : at + 1;
    return true;
}
