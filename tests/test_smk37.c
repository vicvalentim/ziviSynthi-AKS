#include "zivi_smk37.h"
#include "midi_clock.h"
#include "usb_pcm.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_usb_playback(void)
{
    ZiviUsbPcm pcm = {0};
    int32_t out[2 * USB_PCM_BLOCK] = {0};
    uint8_t packet[USB_PCM_PACKET];
    for (unsigned i = 0; i < sizeof packet; i += 4) {
        packet[i] = 0;
        packet[i + 1] = 0x40;
        packet[i + 2] = 0;
        packet[i + 3] = 0xc0;
    }
    pcm_receive(&pcm, packet, USB_PCM_PACKET - 1);
    assert(pcm.write == 0);
    for (unsigned i = 0; i < 5; ++i) pcm_receive(&pcm, packet, sizeof packet);
    pcm_mix(&pcm, out, 64);
    assert(!pcm.primed && out[0] == 0);
    pcm_receive(&pcm, packet, sizeof packet);
    pcm_mix(&pcm, out, 64);
    assert(pcm.primed && pcm.read == USB_PCM_BLOCK);
    for (unsigned i = 0; i < USB_PCM_BLOCK; ++i) {
        assert(out[2 * i] == 2097152 && out[2 * i + 1] == -2097152);
        out[2 * i] = 7000000;
        out[2 * i + 1] = -7000000;
    }
    pcm_mix(&pcm, out, 128);
    assert(pcm.read == 255 && out[0] == 8388607 && out[1] == -8388608);
    pcm_mix(&pcm, out, 128);
    assert(!pcm.primed && pcm.underruns == 1);
    pcm_reset(&pcm);
    for (unsigned i = 0; i < 32; ++i) pcm_receive(&pcm, packet, sizeof packet);
    assert(pcm.overruns && pcm.write <= USB_PCM_CAPACITY);
}

static void test_midi_clock(void)
{
    SmkMidiClock clock = {.external = true};
    Ks ks;
    ks_init(&ks, 44100);
    const KsTimedEvent phrase[] = {{0, 60, 100}, {120, 60, 0}, {450, 67, 96}, {620, 67, 0}};
    assert(ks_load_ms(&ks, phrase, 4, 1000));
    midi_clock_input(&clock, &ks, 0xfa, 0, 0, 1);
    assert(ks.mode == KS_STOPPED);
    midi_clock_input(&clock, &ks, 0xf8, 0, 0, 20);
    assert(ks.mode == KS_PLAYING && ks.position == 0);
    for (unsigned i = 1; i < 30; ++i)
        midi_clock_input(&clock, &ks, 0xf8, 0, 0, 20 + (i * 125) / 6);
    assert(clock.locked && ks.speed >= 99 && ks.speed <= 101);
    midi_clock_input(&clock, &ks, 0xf2, 4, 0, 630);
    ks_prepare_sample(&ks);
    assert(ks.note == 67 && ks.velocity == 96);
    midi_clock_input(&clock, &ks, 0xfc, 0, 0, 660);
    assert(ks.mode == KS_STOPPED);
    midi_clock_input(&clock, &ks, 0xfb, 0, 0, 680);
    midi_clock_input(&clock, &ks, 0xf8, 0, 0, 690);
    assert(ks.mode == KS_PLAYING && ks.position / 100u == 22050);
    midi_clock_timeout(&clock, &ks, 1191);
    assert(ks.mode == KS_STOPPED && !clock.locked);
}

#ifdef ZIVI_REAL32
#include "dsp_curve.h"
#endif

#ifdef ZIVI_REAL32
static void test_curves(void)
{
    for (unsigned i = 0; i <= 4096; ++i) {
        double unit = i / 4096.0;
        double exponent = 18 * unit - 9;
        assert(fabs(dsp_sine((float)unit) - sin(6.283185307179586 * unit)) < 0.000021);
        assert(fabs(dsp_exp2((float)exponent) / exp2(exponent) - 1) < 0.000003);
        double g = tan(3.141592653589793 * 0.22 * unit);
        assert(fabs(dsp_filter_g((float)(0.22 * unit)) - g / (1 + g)) < 0.000002);
    }
    for (unsigned frames = 132; frames < 662000; frames += 127) {
        double reference = -expm1(-1.0 / frames);
        assert(fabs(dsp_decay_coeff((float)frames) / reference - 1) < 0.000001);
    }
}

static void test_catalog_audio(void)
{
    ZiviSmk37 engine;
    int32_t pcm[ZIVI_SMK_WORDS];
    for (unsigned preset = 0; preset < synth_factory_count(); ++preset) {
        zivi_smk_init(&engine);
        assert(synth_factory_apply(&engine.synth, preset));
        if (engine.synth.ks.sequence_trigger) assert(ks_play(&engine.synth.ks));
        else assert(zivi_smk_key(&engine, 12, 100, 0));
        unsigned peak = 0;
        for (unsigned block = 0; block < 768; ++block) {
            assert(zivi_smk_fill(&engine) && zivi_smk_irq_read(&engine, pcm));
            for (unsigned i = 0; i < ZIVI_SMK_WORDS; ++i) {
                int32_t sample = pcm[i];
                assert(sample >= -8388607 && sample <= 8388607);
                unsigned magnitude = (unsigned)(sample < 0 ? -sample : sample);
                if (magnitude > peak) peak = magnitude;
            }
            assert(isfinite(engine.synth.modules.env));
            assert(isfinite(engine.synth.meter_peak));
        }
        if (peak <= 800) fprintf(stderr, "Peak too low: %s (%u)\n", synth_factory_display(preset), peak);
        assert(peak > 800 && engine.underruns == 0);
    }
    printf("DSP 32 bits: curvas verificadas e %u presets renderizados.\n", synth_factory_count());
}
#endif

static void test_patch_transition(void)
{
    static ZiviSmk37 engine, reference;
    int32_t transition[ZIVI_SMK_WORDS], unmodified[ZIVI_SMK_WORDS];
    zivi_smk_init(&engine);
    assert(zivi_smk_key(&engine, 21, 120, 0));
    for (unsigned i = 0; i < 4; ++i)
        assert(zivi_smk_fill(&engine) && zivi_smk_irq_read(&engine, transition));
    int32_t tail[2] = {transition[ZIVI_SMK_WORDS - 2], transition[ZIVI_SMK_WORDS - 1]};
    assert(tail[0] != 0);
    assert(synth_factory_apply(&engine.synth, 13));
    reference = engine;
    engine.patch_transition = true;
    assert(zivi_smk_fill(&engine) && zivi_smk_irq_read(&engine, transition));
    assert(zivi_smk_fill(&reference) && zivi_smk_irq_read(&reference, unmodified));
    for (int frame = 0; frame < 32; ++frame)
        for (unsigned channel = 0; channel < 2; ++channel) {
            unsigned word = 2 * frame + channel;
            assert(transition[word] == (tail[channel] * (32 - frame) + unmodified[word] * frame) / 32);
        }
    assert(memcmp(transition + 64, unmodified + 64, (ZIVI_SMK_WORDS - 64) * sizeof *transition) == 0);
    assert(!engine.patch_transition);
    assert(!zivi_smk_irq_read(&engine, transition));
    engine.patch_transition = true;
    assert(zivi_smk_fill(&engine) && zivi_smk_irq_read(&engine, transition));
    assert(transition[0] == 0 && transition[1] == 0);
}

#ifdef ZIVI_SMK_OPEN_AUDIO
static void test_smooth_gap(void)
{
    static ZiviSmk37 engine;
    int32_t pcm[ZIVI_SMK_WORDS];
    zivi_smk_init(&engine);
    engine.ring[0][ZIVI_SMK_WORDS - 2] = 8388607;
    engine.ring[0][ZIVI_SMK_WORDS - 1] = -8388608;
    engine.write = 1;
    assert(zivi_smk_irq_read(&engine, pcm));
    assert(!zivi_smk_irq_read(&engine, pcm));
    assert(pcm[0] == 8388607 && pcm[1] == -8388608);
    assert(pcm[62] > 0 && pcm[63] < 0 && pcm[64] == 0);
    assert(!zivi_smk_irq_read(&engine, pcm));
    for (unsigned i = 0; i < ZIVI_SMK_WORDS; ++i) assert(pcm[i] == 0);
    assert(engine.underruns == 2);
    for (unsigned i = 0; i < ZIVI_SMK_WORDS; ++i) engine.ring[1][i] = i & 1 ? -100000 : 100000;
    engine.write = 2;
    assert(zivi_smk_irq_read(&engine, pcm));
    assert(pcm[0] == 0 && pcm[1] == 0);
    assert(pcm[32] == 50000 && pcm[33] == -50000);
    assert(pcm[64] == 100000 && pcm[65] == -100000);
}
static void test_codec_rate_and_dc(void)
{
    static ZiviSmk37 engine;
    int32_t pcm[ZIVI_SMK_WORDS];
    zivi_smk_init(&engine);
    synth_clear_matrix(&engine.synth);
    assert(synth_pin(&engine.synth, SRC_INPUT2, DST_OUT1_IN, PIN_FULL));
    assert(zivi_smk_key(&engine, 21, 127, 0));
    assert(zivi_smk_fill(&engine) && zivi_smk_irq_read(&engine, pcm));
    assert(pcm[0] > 200000);
    for (unsigned i = 0; i < 256; ++i)
        assert(zivi_smk_fill(&engine) && zivi_smk_irq_read(&engine, pcm));
    assert(abs(pcm[0]) < 90);

    zivi_smk_init(&engine);
    synth_clear_matrix(&engine.synth);
    assert(synth_pin(&engine.synth, SRC_OSC1, DST_OUT1_IN, PIN_FULL));
    synth_module_control(&engine.synth, M4_OSC1_HZ, 440);
    synth_module_control(&engine.synth, M4_SINE_LEVEL, 1);
    synth_module_control(&engine.synth, M4_RAMP_LEVEL, 0);
    unsigned first = 0, last = 0, crossings = 0, at = 0;
    int32_t previous = 0;
    for (unsigned block = 0; block < 128; ++block) {
        assert(zivi_smk_fill(&engine) && zivi_smk_irq_read(&engine, pcm));
        for (unsigned frame = 0; frame < ZIVI_SMK_FRAMES; ++frame, ++at) {
            int32_t current = pcm[2 * frame];
            if (previous < 0 && current >= 0) {
                if (!crossings) first = at;
                last = at;
                ++crossings;
            }
            previous = current;
        }
    }
    assert(crossings > 150 && last > first);
    double hz = (double)(crossings - 1) * ZIVI_SMK_RATE / (last - first);
    assert(fabs(hz - 440) < 0.1);
    printf("DSP Open: A4 %.3f Hz a %u Hz nominais e teste de rejeição DC aprovado.\n",
           hz, (unsigned)ZIVI_SMK_RATE);
}
#endif

static void test_q23_limits(void)
{
    static ZiviSmk37 engine;
    int32_t pcm[ZIVI_SMK_WORDS];
    zivi_smk_init(&engine);
    engine.gain_q12 = 1u << 22;
    assert(zivi_smk_key(&engine, 21, 127, 0));
    bool high = false, low = false;
    for (unsigned block = 0; block < 40; ++block) {
        assert(zivi_smk_fill(&engine) && zivi_smk_irq_read(&engine, pcm));
        for (unsigned i = 0; i < ZIVI_SMK_WORDS; ++i) {
#ifdef ZIVI_SMK_OPEN_AUDIO
            assert(pcm[i] > -67108864 && pcm[i] < 67108864);
            high |= pcm[i] > 8388607;
            low |= pcm[i] < -8388608;
#else
            assert(pcm[i] >= -8388608 && pcm[i] <= 8388607);
            high |= pcm[i] == 8388607;
            low |= pcm[i] == -8388608;
#endif
        }
    }
    assert(high && low);
}

#if ZIVI_SMK_MIDI_PORTS > 1
static void test_midi_ports(void)
{
    static ZiviSmk37 engine;
    zivi_smk_init(&engine);
    assert(zivi_smk_midi_port(&engine, 0, 0x90, 60, 90));
    assert(zivi_smk_midi_port(&engine, 1, 0x90, 60, 100));
    assert(zivi_smk_midi_port(&engine, 0, 0xb0, 64, 127));
    assert(zivi_smk_midi_port(&engine, 0, 0x80, 60, 0));
    zivi_smk_midi_port_disconnect(&engine, 1);
    assert(engine.synth.held_count == 1);
    assert(zivi_smk_midi_port(&engine, 1, 0x90, 65, 100));
    assert(zivi_smk_midi_port(&engine, 0, 0xb0, 64, 0));
    assert(engine.synth.held_count == 1 && engine.synth.held[0] == 65);
    assert(zivi_smk_key(&engine, 17, 95, 0));
    zivi_smk_midi_port_disconnect(&engine, 1);
    assert(engine.synth.held_count == 1 && engine.synth.held[0] == 65);
    assert(zivi_smk_key(&engine, 17, 0, 0));
    assert(engine.synth.held_count == 0);
}
#endif

int main(void)
{
    test_q23_limits();
    test_patch_transition();
#ifdef ZIVI_SMK_OPEN_AUDIO
    test_smooth_gap();
    test_codec_rate_and_dc();
#endif
#if ZIVI_SMK_MIDI_PORTS > 1
    test_midi_ports();
#endif
    test_usb_playback();
    test_midi_clock();
    ZiviSmk37 engine;
    zivi_smk_init(&engine);
    assert(engine.synth.rate == ZIVI_SMK_RATE);
    assert(engine.synth.modules.spring_length_a == (ZIVI_SMK_RATE * 25 + 500) / 1000);
    assert(engine.synth.modules.spring_length_b == (ZIVI_SMK_RATE * 30 + 500) / 1000);
    assert(!synth_init(&engine.synth, 48000));
    int32_t pcm[ZIVI_SMK_WORDS];
    assert(!zivi_smk_irq_read(&engine, pcm) && engine.underruns == 1);
    for (unsigned i = 0; i < ZIVI_SMK_WORDS; ++i) assert(pcm[i] == 0);

    assert(!zivi_smk_key(&engine, 37, 100, 0));
    assert(zivi_smk_key(&engine, 21, 104, 0));
    assert(engine.key_note[21] == 69);
    for (unsigned i = 0; i < ZIVI_SMK_SLOTS - 1; ++i) assert(zivi_smk_fill(&engine));
    assert(!zivi_smk_fill(&engine));
    assert(zivi_smk_irq_read(&engine, pcm));
    bool nonzero = false;
    for (unsigned i = 0; i < ZIVI_SMK_FRAMES; ++i) {
        nonzero |= pcm[2 * i] != 0;
        assert(pcm[2 * i + 1] == pcm[2 * i]);
    }
    assert(nonzero);
    assert(zivi_smk_fill(&engine) && zivi_smk_irq_read(&engine, pcm));
    for (unsigned i = 0; i < ZIVI_SMK_SLOTS - 2; ++i) assert(zivi_smk_irq_read(&engine, pcm));
    assert(!zivi_smk_irq_read(&engine, pcm));
    assert(engine.underruns == 2);

    assert(zivi_smk_key(&engine, 21, 0, 1));
    assert(engine.synth.held_count == 0);
    assert(zivi_smk_key(&engine, 21, 110, 0));
    assert(zivi_smk_key(&engine, 9, 100, 1));
    assert(zivi_smk_midi(&engine, 0x92, 69, 90));
    assert(zivi_smk_midi(&engine, 0x95, 69, 80));
    assert(zivi_smk_key(&engine, 21, 0, 0));
    assert(zivi_smk_midi(&engine, 0xb2, 123, 0));
    assert(engine.synth.held_count == 1);
    zivi_smk_midi_disconnect(&engine);
    assert(engine.synth.held_count == 1);
    assert(zivi_smk_key(&engine, 9, 0, 0));
    assert(engine.synth.held_count == 0);
    assert(zivi_smk_midi(&engine, 0x91, 69, 90));
    assert(zivi_smk_midi(&engine, 0xb1, 64, 127));
    assert(zivi_smk_midi(&engine, 0x81, 69, 0));
    assert(engine.synth.held_count == 1);
    assert(zivi_smk_midi(&engine, 0xb2, 64, 0));
    assert(engine.synth.held_count == 1);
    assert(zivi_smk_midi(&engine, 0xb1, 64, 0));
    assert(engine.synth.held_count == 0);
    engine.synth.midi_channel = 2;
    assert(!zivi_smk_midi(&engine, 0x90, 69, 90));
    assert(zivi_smk_midi(&engine, 0x91, 69, 90));
    zivi_smk_midi_disconnect(&engine);
    assert(engine.synth.held_count == 0);
    engine.synth.midi_channel = 0;
    assert(!zivi_smk_pad(&engine, 16, 0, 8, PIN_FULL));
    assert(zivi_smk_pad(&engine, 13, 0, 8, PIN_FULL));
    assert(zivi_smk_pad(&engine, 12, 0, 4, PIN_HALF));
    assert(engine.synth.matrix[13][0] == PIN_HALF);
    assert(zivi_smk_pad(&engine, 12, 0, 4, PIN_HALF));
    assert(engine.synth.matrix[13][0] == PIN_OFF);

    assert(synth_factory_count() == 62);
    assert(synth_factory_apply(&engine.synth, (unsigned)synth_factory_find("pulso-em-transito")));
    assert(engine.synth.ks.count == 32 && engine.synth.ks.length == (ZIVI_SMK_RATE * 1664 + 500) / 1000);
    const KsTimedEvent invalid[] = {{0, 60, 100}, {30, 61, 0}};
    assert(!ks_load_ms(&engine.synth.ks, invalid, 2, 40));
    assert(engine.synth.ks.count == 32 && engine.synth.ks.length == (ZIVI_SMK_RATE * 1664 + 500) / 1000);
    assert(ks_play(&engine.synth.ks));
    assert(zivi_smk_fill(&engine) && zivi_smk_irq_read(&engine, pcm));
    assert(engine.synth.ks.note == 52);
    assert(fabs(engine.synth.hz[1] - 131.103) < 1);
    zivi_smk_panic(&engine);
    assert(engine.synth.ks.mode == KS_STOPPED && engine.synth.modules.phase == ENV_OFF);

    engine.muted = true;
    assert(zivi_smk_fill(&engine) && zivi_smk_irq_read(&engine, pcm));
    for (unsigned i = 0; i < ZIVI_SMK_WORDS; ++i) assert(pcm[i] == 0);

#ifdef ZIVI_REAL32
    test_curves();
    test_catalog_audio();
#endif
    printf("SMK-37 Pro: %u Hz, blocos 128, Q23 estéreo, fila, pads, sustain, clock e áudio USB aprovados.\n", (unsigned)ZIVI_SMK_RATE);
    printf("Host: sizeof(Synth) = %zu B, sizeof(ZiviSmk37) = %zu B.\n",
           sizeof engine.synth, sizeof engine);
    return 0;
}
