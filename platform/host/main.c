#include "presets.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SAMPLE_RATE = 48000, BLOCK = 128 };

static void le16(FILE *stream, unsigned value)
{
    fputc((int)(value & 255u), stream);
    fputc((int)((value >> 8) & 255u), stream);
}

static void le32(FILE *stream, unsigned value)
{
    le16(stream, value & 65535u);
    le16(stream, value >> 16);
}

static void wav_header(FILE *stream, unsigned frames, unsigned channels)
{
    fputs("RIFF", stream);
    le32(stream, 36u + frames * 2u * channels);
    fputs("WAVEfmt ", stream);
    le32(stream, 16);
    le16(stream, 1);
    le16(stream, channels);
    le32(stream, SAMPLE_RATE);
    le32(stream, SAMPLE_RATE * 2 * channels);
    le16(stream, 2 * channels);
    le16(stream, 16);
    fputs("data", stream);
    le32(stream, frames * 2 * channels);
}

static void write_block(FILE *stream, Synth *synth, unsigned frames)
{
    float samples[BLOCK];
    while (frames > 0) {
        unsigned length = frames > BLOCK ? BLOCK : frames;
        synth_render(synth, samples, length);
        for (unsigned i = 0; i < length; ++i) {
            int pcm = (int)(samples[i] * 32767.0f);
            le16(stream, (uint16_t)pcm);
        }
        frames -= length;
    }
}

static void preview_stereo(FILE *stream, Synth *synth, unsigned frames, bool fade)
{
    float left[BLOCK], right[BLOCK];
    while (frames) {
        unsigned length = frames > BLOCK ? BLOCK : frames;
        synth_render_stereo(synth, left, right, length);
        for (unsigned i = 0; i < length; ++i) {
            float gain = fade && frames - i < 1440 ? (frames - i - 1) / 1440.0f : 1.0f;
            le16(stream, (uint16_t)(int)(left[i] * gain * 32767.0f));
            le16(stream, (uint16_t)(int)(right[i] * gain * 32767.0f));
        }
        frames -= length;
    }
}

static bool preview_usb(FILE *stream, Synth *synth)
{
    bool routed = false;
    for (unsigned ch = 0; ch < 2; ++ch)
        for (unsigned col = 0; col < MATRIX_SIZE; ++col)
            routed |= synth->usb_routes[ch][col] != PIN_OFF;
    if (!routed) return false;

    const unsigned total = 6 * SAMPLE_RATE;
    float input[BLOCK], left[BLOCK], right[BLOCK];
    for (unsigned offset = 0; offset < total; offset += BLOCK) {
        unsigned length = total - offset < BLOCK ? total - offset : BLOCK;
        for (unsigned i = 0; i < length; ++i)
            input[i] = 0.25f * sinf((offset + i) * (6.283185307179586f * 220 / SAMPLE_RATE));
        synth_render_usb(synth, left, right, length, input, input);
        for (unsigned i = 0; i < length; ++i) {
            le16(stream, (uint16_t)(int)(left[i] * 32767.0f));
            le16(stream, (uint16_t)(int)(right[i] * 32767.0f));
        }
    }
    return true;
}

static void preview_factory(FILE *stream, Synth *synth, unsigned note)
{
    wav_header(stream, 6 * SAMPLE_RATE, 2);
    if (preview_usb(stream, synth)) return;
    if (synth->ks.sequence_trigger && ks_play(&synth->ks)) {
        const bool duet = synth->matrix[SRC_INPUT2][DST_OUT2_LEVEL] != PIN_OFF;
        if (duet) {
            synth_midi(synth, 0x90, note, 104);
            preview_stereo(stream, synth, SAMPLE_RATE * 2, false);
            synth_midi(synth, 0x80, note, 0);
            synth_midi(synth, 0x90, note <= 120 ? note + 7 : note - 7, 96);
            preview_stereo(stream, synth, SAMPLE_RATE * 2, false);
            synth_midi(synth, 0x80, note <= 120 ? note + 7 : note - 7, 0);
            preview_stereo(stream, synth, SAMPLE_RATE, false);
        } else preview_stereo(stream, synth, SAMPLE_RATE * 5, false);
        ks_stop(&synth->ks);
        preview_stereo(stream, synth, SAMPLE_RATE, true);
    } else if (synth->modules.p[M4_ENV_MODE] == 2) {
        preview_stereo(stream, synth, SAMPLE_RATE * 6, true);
    } else if (synth->modules.p[M4_ATTACK_MS] >= 100) {
        synth_midi(synth, 0x90, note, 112);
        preview_stereo(stream, synth, SAMPLE_RATE * 4, false);
        synth_midi(synth, 0x80, note, 0);
        preview_stereo(stream, synth, SAMPLE_RATE * 2, true);
    } else {
        const unsigned intervals[] = {0, 7, 3, 10, 5, 0};
        for (unsigned i = 0; i < sizeof intervals / sizeof intervals[0]; ++i) {
            unsigned pitch = note + intervals[i] <= 127 ? note + intervals[i] : note - intervals[i];
            synth_midi(synth, 0x90, pitch, 112u - (i % 3) * 10u);
            preview_stereo(stream, synth, SAMPLE_RATE / 3, false);
            synth_midi(synth, 0x80, pitch, 0);
            preview_stereo(stream, synth, SAMPLE_RATE - SAMPLE_RATE / 3, i == 5);
        }
    }
}

int main(int argc, char **argv)
{
    bool aks = argc > 1 && strcmp(argv[1], "--aks") == 0;
    if (aks) {
        --argc;
        ++argv;
    }
    if (argc < 2 || argc > 4 || strcmp(argv[1], "--help") == 0) {
        fputs("ziviSynthi AKS " ZIVI_VERSION "\n"
              "Uso: zivisynthi-host [--aks] saida.wav [nota MIDI 0..127] [sine|ramp|half|off|filter|ring|spring|resonance|repeat|preset]\n", stderr);
        return argc == 2 ? 0 : 2;
    }

    unsigned note = 69;
    if (argc >= 3) {
        char *end;
        errno = 0;
        long parsed = strtol(argv[2], &end, 10);
        if (errno || end == argv[2] || *end || parsed < 0 || parsed > 127) {
            fputs("Nota MIDI inválida.\n", stderr);
            return 2;
        }
        note = (unsigned)parsed;
    }

    Synth synth;
    if (!synth_init(&synth, SAMPLE_RATE)) return 1;
    modules_set_profile(&synth.modules, SAMPLE_RATE, aks);
    int factory = argc == 4 ? synth_factory_find(argv[3]) : -1;
    if (factory >= 0 && !synth_factory_apply(&synth, (unsigned)factory)) return 1;
    if (argc == 4 && factory < 0) {
        const char *route = argv[3];
        if (strcmp(route, "ramp") == 0) {
            synth_patch_controls(&synth, 1.0, synth.lfo, synth.cutoff);
        } else if (strcmp(route, "half") == 0) {
            synth_pin(&synth, SRC_ENVELOPE, DST_OUT1_IN, PIN_HALF);
            synth_pin(&synth, SRC_ENVELOPE, DST_OUT2_IN, PIN_HALF);
        } else if (strcmp(route, "off") == 0) {
            synth_pin(&synth, SRC_ENVELOPE, DST_OUT1_IN, PIN_OFF);
            synth_pin(&synth, SRC_ENVELOPE, DST_OUT2_IN, PIN_OFF);
        } else if (strcmp(route, "filter") == 0 || strcmp(route, "ring") == 0) {
            bool filtered = strcmp(route, "filter") == 0;
            synth_pin(&synth, SRC_OSC1, DST_ENV_IN, PIN_OFF);
            synth_pin(&synth, SRC_OSC1, filtered ? DST_FILTER_IN : DST_RING_A, PIN_FULL);
            if (!filtered) synth_pin(&synth, SRC_OSC3_TRI, DST_RING_B, PIN_FULL);
            synth_pin(&synth, filtered ? SRC_FILTER : SRC_RING, DST_ENV_IN, PIN_FULL);
            synth_patch_controls(&synth, 1.0, 110.0, 560.0);
        } else if (strcmp(route, "spring") == 0) {
            synth_spring_patch(&synth);
        } else if (strcmp(route, "repeat") == 0) {
            synth_repeat_patch(&synth);
        } else if (strcmp(route, "resonance") == 0) {
            synth_clear_matrix(&synth);
            synth_pin(&synth, SRC_NOISE, DST_FILTER_IN, PIN_HALF);
            synth_pin(&synth, SRC_OSC3_TRI, DST_FILTER_CUTOFF, PIN_HALF);
            synth_pin(&synth, SRC_FILTER, DST_ENV_IN, PIN_FULL);
            synth_pin(&synth, SRC_ENVELOPE, DST_OUT1_IN, PIN_FULL);
            synth_pin(&synth, SRC_ENVELOPE, DST_OUT2_IN, PIN_FULL);
            synth_patch_controls(&synth, 0.0, 0.35, 650.0);
            synth_module_control(&synth, M4_RESPONSE, 0.81);
            synth_module_control(&synth, M4_FILTER_DRIVE, 1.6);
        } else if (strcmp(route, "sine") != 0) {
            fputs("Rota inválida.\n", stderr);
            return 2;
        }
    }

    FILE *stream = fopen(argv[1], "wb");
    if (!stream) {
        perror(argv[1]);
        return 1;
    }
    if (factory >= 0) {
        preview_factory(stream, &synth, note);
    } else {
        const unsigned hold = SAMPLE_RATE * 3 / 4;
        const unsigned release = SAMPLE_RATE / 4;
        wav_header(stream, hold + release, 1);
        synth_midi(&synth, 0x90, note, 110);
        write_block(stream, &synth, hold);
        synth_midi(&synth, 0x80, note, 0);
        write_block(stream, &synth, release);
    }
    int failed = ferror(stream);
    if (fclose(stream) != 0 || failed) {
        fprintf(stderr, "Falha de escrita em %s\n", argv[1]);
        return 1;
    }
    printf("ziviSynthi AKS " ZIVI_VERSION " · WAV gravado em %s\n", argv[1]);
    return 0;
}
