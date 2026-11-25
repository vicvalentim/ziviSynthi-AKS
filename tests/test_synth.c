#include "synth.h"
#include "flash_policy.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FRAMES 4800

static float samples[FRAMES];

static double rms(void)
{
    double energy = 0;
    for (size_t i = 0; i < FRAMES; ++i) {
        assert(isfinite(samples[i]) && fabsf(samples[i]) < 0.5f);
        energy += samples[i] * samples[i];
    }
    return sqrt(energy / FRAMES);
}

static unsigned rising_crossings(void)
{
    unsigned crossings = 0;
    for (size_t i = 1; i < FRAMES; ++i) {
        if (samples[i - 1] <= 0 && samples[i] > 0) ++crossings;
    }
    return crossings;
}

static void test_matrix(void)
{
    Synth synth;
    assert(synth_init(&synth, 48000));
    unsigned supported = 0;
    for (unsigned row = 0; row < MATRIX_SIZE; ++row) {
        for (unsigned col = 0; col < MATRIX_SIZE; ++col) {
            bool accept = synth_pin(&synth, row, col, PIN_FULL);
            assert(accept == synth_pin_supported(row, col));
            supported += accept;
        }
    }
    assert(supported == 256);
    assert(!synth_pin(&synth, 16, 0, PIN_FULL));
    assert(!synth_pin(&synth, 0, 16, PIN_FULL));
    assert(!synth_pin(&synth, SRC_OSC1, DST_OUT1_IN, (enum Pin)3));
    assert(strcmp(synth_source_name(6), "NOISE") == 0);
    assert(strcmp(synth_dest_name(7), "FILTER") == 0);
    assert(synth_pin_delayed(SRC_FILTER, DST_FILTER_IN));
    assert(!synth_pin_delayed(SRC_OSC1, DST_FILTER_IN));

    synth_safe_patch(&synth);
    assert(synth_pin(&synth, SRC_ENVELOPE, DST_OUT2_IN, PIN_OFF));
    assert(synth_midi(&synth, 0x90, 69, 127));
    synth_render(&synth, samples, FRAMES);
    synth_render(&synth, samples, FRAMES);
    double full = rms();
    assert(full > 0.1 && rising_crossings() >= 43 && rising_crossings() <= 45);
    assert(synth_toggle_pin(&synth, SRC_ENVELOPE, DST_OUT1_IN, PIN_HALF));
    synth_render(&synth, samples, FRAMES);
    assert(fabs(rms() / full - 0.5) < 0.001);
    assert(synth_toggle_pin(&synth, SRC_ENVELOPE, DST_OUT1_IN, PIN_HALF));
    synth_render(&synth, samples, FRAMES);
    assert(rms() == 0);

    assert(synth_pin(&synth, SRC_OSC1, DST_OUT2_IN, PIN_FULL));
    float right[FRAMES];
    synth_render_stereo(&synth, samples, right, FRAMES);
    assert(rms() == 0 && fabsf(right[1]) > 0);
    synth_clear_matrix(&synth);
    assert(synth_pin(&synth, SRC_OSC1, DST_METER, PIN_FULL));
    synth_render(&synth, samples, FRAMES);
    assert(rms() == 0 && synth.meter_peak > 0.99);

    assert(synth_pin(&synth, SRC_OSC1, DST_OUT1_IN, PIN_FULL));
    assert(synth_pin(&synth, SRC_TRAPEZOID, DST_OUT1_LEVEL, PIN_HALF));
    synth_render(&synth, samples, FRAMES);
    assert(fabs(rms() / full - 0.5) < 0.001);
}

static void test_chains_and_feedback(void)
{
    Synth reference, routed;
    assert(synth_init(&reference, 48000));
    assert(synth_init(&routed, 48000));
    synth_clear_matrix(&reference);
    synth_clear_matrix(&routed);
    assert(synth_pin(&reference, SRC_OSC1, DST_OUT1_IN, PIN_FULL));
    synth_render(&reference, samples, FRAMES);
    double original = rms();

    assert(synth_pin(&routed, SRC_OSC1, DST_FILTER_IN, PIN_FULL));
    assert(synth_pin(&routed, SRC_FILTER, DST_OUT1_IN, PIN_FULL));
    synth_patch_controls(&routed, 0, routed.lfo, 300);
    synth_render(&routed, samples, FRAMES);
    double filtered = rms();
    assert(filtered > 0.01 && filtered < original / 3);
    reference = routed;
    float dry[FRAMES];
    synth_render(&reference, dry, FRAMES);
    assert(synth_pin(&routed, SRC_FILTER, DST_FILTER_IN, PIN_HALF));
    synth_render(&routed, samples, FRAMES);
    double feedback_difference = 0;
    for (unsigned i = 0; i < FRAMES; ++i) {
        feedback_difference = fmax(feedback_difference, fabs(samples[i] - dry[i]));
    }
    assert(feedback_difference > 0.001);
    assert(synth_pin(&routed, SRC_OUT1, DST_FILTER_CUTOFF, PIN_FULL));
    assert(synth_pin(&routed, SRC_OSC3_TRI, DST_FILTER_CUTOFF, PIN_HALF));
    assert(synth_pin(&routed, SRC_NOISE, DST_FILTER_IN, PIN_HALF));
    for (unsigned i = 0; i < 60; ++i) {
        synth_render(&routed, samples, FRAMES);
        assert(isfinite(rms()) && fabs(routed.signals[SRC_FILTER]) <= 1);
    }

    synth_clear_matrix(&routed);
    assert(synth_pin(&routed, SRC_OSC1, DST_RING_A, PIN_FULL));
    assert(synth_pin(&routed, SRC_OSC3_TRI, DST_RING_B, PIN_FULL));
    assert(synth_pin(&routed, SRC_RING, DST_OUT1_IN, PIN_FULL));
    synth_render(&routed, samples, FRAMES);
    assert(rms() > 0.02);
    assert(synth_pin(&routed, SRC_OSC3_TRI, DST_RING_B, PIN_OFF));
    synth_render(&routed, samples, FRAMES);
    assert(rms() == 0);

    Synth same;
    assert(synth_init(&same, 48000));
    assert(synth_init(&routed, 48000));
    synth_clear_matrix(&same);
    synth_clear_matrix(&routed);
    for (unsigned row = 0; row < MATRIX_SIZE; ++row) {
        for (unsigned col = 0; col < MATRIX_SIZE; ++col) {
            if (!synth_pin_supported(row, col)) continue;
            assert(synth_pin(&routed, row, col, PIN_FULL));
        }
    }
    for (unsigned row = MATRIX_SIZE; row--; ) {
        for (unsigned col = MATRIX_SIZE; col--; ) {
            if (!synth_pin_supported(row, col)) continue;
            assert(synth_pin(&same, row, col, PIN_FULL));
        }
    }
    synth_midi(&same, 0x90, 60, 127);
    synth_midi(&routed, 0x90, 60, 127);
    float comparable[FRAMES];
    for (unsigned i = 0; i < 10; ++i) {
        synth_render(&routed, samples, FRAMES);
        synth_render(&same, comparable, FRAMES);
        assert(memcmp(samples, comparable, sizeof samples) == 0);
        assert(isfinite(rms()));
    }
}

static void test_midi(void)
{
    Synth synth;
    assert(!synth_init(&synth, NAN));
    assert(!synth_init(&synth, 7999));
    assert(synth_init(&synth, 48000));
    synth_midi_channel(&synth, 3);
    assert(!synth_midi(&synth, 0x90, 69, 127));
    assert(!synth_midi(&synth, 0x92, 128, 127));
    assert(synth_midi(&synth, 0x92, 69, 127));
    assert(synth_midi(&synth, 0x92, 81, 100));
    synth_render(&synth, samples, FRAMES);
    assert(rising_crossings() >= 87 && rising_crossings() <= 89);
    assert(synth_pin(&synth, SRC_INPUT1, DST_OSC1_PITCH, PIN_OFF));
    synth_render(&synth, samples, FRAMES);
    assert(rising_crossings() >= 43 && rising_crossings() <= 45);
    assert(synth_pin(&synth, SRC_INPUT1, DST_OSC1_PITCH, PIN_FULL));
    assert(synth_midi(&synth, 0x92, 81, 0));
    synth_render(&synth, samples, FRAMES);
    assert(synth.pitch_note == 69 && synth.held_count == 1);
    assert(synth_midi(&synth, 0xe2, 127, 127) && synth.bend == 2);
    assert(synth_midi(&synth, 0xb2, 1, 80) && synth.shape == 80.0 / 127);
    assert(synth_midi(&synth, 0xb2, 7, 100) && synth.level == 100.0 / 127);
    assert(synth_midi(&synth, 0x82, 69, 0));
    synth_render(&synth, samples, FRAMES);
    synth_render(&synth, samples, FRAMES);
    assert(rms() < 0.001 && synth.held_count == 0);
    assert(synth_midi(&synth, 0xb2, 123, 0) && synth.bend == 0);

    for (unsigned note = 0; note < MIDI_NOTES; ++note) {
        assert(synth_midi(&synth, 0x92, note, 90));
    }
    assert(synth.held_count == MIDI_NOTES);
    assert(synth_midi(&synth, 0x92, 0, 100) && synth.held_count == MIDI_NOTES);
    assert(synth.held[MIDI_NOTES - 1] == 0);
}

static void advance_ks(Ks *ks, unsigned frames)
{
    for (unsigned i = 0; i < frames; ++i) {
        ks_prepare_sample(ks);
        ks_advance(ks);
    }
}

static void test_transport(void)
{
    Ks ks;
    ks_init(&ks, 8000);
    assert(!ks_play(&ks));
    ks_record(&ks);
    advance_ks(&ks, 9000);
    assert(ks.mode == KS_RECORDING && ks.record_frame == 0);
    ks_capture(&ks, 60, 108, true);
    advance_ks(&ks, 73);
    ks_capture(&ks, 60, 0, false);
    advance_ks(&ks, 21);
    ks_capture(&ks, 63, 91, true);
    advance_ks(&ks, 69);
    assert(ks_play(&ks));
    assert(ks.length == 163 && ks.count == 3);
    assert(ks.events[0].frame == 0 && ks.events[1].frame == 73 && ks.events[2].frame == 94);
    assert(ks.events[0].attack && !ks.events[1].attack);

    ks_controls(&ks, 100, 100, 200, 0, 12, false, false);
    ks_prepare_sample(&ks);
    assert(ks.note == 60 && ks.velocity == 108 && ks.attack);
    advance_ks(&ks, 37);
    ks_prepare_sample(&ks);
    assert(ks.velocity == 0 && !ks.attack);
    advance_ks(&ks, 10);
    ks_prepare_sample(&ks);
    assert(ks.note == 63 && ks.velocity == 91);
    advance_ks(&ks, 35);
    ks_prepare_sample(&ks);
    assert(ks.mode == KS_STOPPED && ks.velocity == 0);
    ks_controls(&ks, 100, 100, 100, 0, 1, true, false);
    assert(ks_play(&ks));
    advance_ks(&ks, 164);
    assert(ks.mode == KS_PLAYING && ks.position == 100 && ks.velocity == 108);

    const KsTimedState legato[] = {{0, 60, 100, true}, {100, 67, 90, true}, {200, 60, 100, false}};
    assert(ks_load_states(&ks, legato, 3, 300));
    assert(ks.count == 3 && ks.events[2].note == 60 && !ks.events[2].attack);

    ks_record(&ks);
    for (unsigned i = 0; i < KS_CAPACITY; ++i) {
        ks_capture(&ks, 69, i & 1u ? 0 : 100, !(i & 1u));
        ks_advance(&ks);
    }
    ks_capture(&ks, 70, 107, true);
    assert(ks.mode == KS_STOPPED && ks.count == KS_CAPACITY && ks.limit == KS_LIMIT_MEMORY);
    assert(ks.length == KS_CAPACITY);
    ks_record(&ks);
    ks_capture(&ks, 60, 100, true);
    advance_ks(&ks, 8000);
    assert(ks.mode == KS_STOPPED && ks.length == 8000 && ks.limit == KS_LIMIT_TIME);

    ks_random(&ks, true);
    unsigned note = ks.random_note;
    assert(ks.random_held && note >= 48 && note <= 77);
    ks_random(&ks, true);
    assert(ks.random_note == note);
    ks_random(&ks, false);
    assert(!ks.random_held);
}

static void test_ks_performance(void)
{
    Synth synth;
    assert(synth_init(&synth, 48000));
    synth_ks_patch(&synth);
    synth_ks_record(&synth);
    assert(synth_midi(&synth, 0x90, 69, 110));
    for (unsigned i = 0; i < 5; ++i) synth_render(&synth, samples, FRAMES);
    assert(synth_midi(&synth, 0x80, 69, 0));
    synth_render(&synth, samples, FRAMES);
    assert(ks_play(&synth.ks));
    assert(synth.ks.count == 2 && synth.ks.length == 28800);
    assert(synth_midi(&synth, 0x90, 57, 110));
    float right[FRAMES];
    synth_render_stereo(&synth, samples, right, FRAMES);
    assert(rms() > 0.07 && rising_crossings() >= 43 && rising_crossings() <= 45);
    memcpy(samples, right, sizeof right);
    assert(rms() > 0.06 && rising_crossings() >= 21 && rising_crossings() <= 23);

    ks_controls(&synth.ks, 100, 100, 100, 12, 12, true, true);
    synth_render_stereo(&synth, samples, right, FRAMES);
    assert(rising_crossings() >= 87 && rising_crossings() <= 89);
    memcpy(samples, right, sizeof right);
    assert(rising_crossings() >= 21 && rising_crossings() <= 23);
    ks_controls(&synth.ks, 50, 50, 100, 12, 12, true, true);
    synth_render_stereo(&synth, samples, right, FRAMES);
    assert(rising_crossings() >= 61 && rising_crossings() <= 63);
    memcpy(samples, right, sizeof right);
    assert(rising_crossings() >= 30 && rising_crossings() <= 32);
    assert(synth_midi(&synth, 0x80, 57, 0));
    synth_render_stereo(&synth, samples, right, FRAMES);
    assert(rms() > 0.07);
    memcpy(samples, right, sizeof right);
    assert(rms() == 0);
    ks_stop(&synth.ks);
    for (unsigned i = 0; i < 3; ++i) synth_render(&synth, samples, FRAMES);
    assert(rms() < 0.001 && synth.ks.velocity == 0);
}

static void test_filter_m4(void)
{
    Synth synth;
    const double rate = 48000.0;
    const double cutoff = 600.0;
    double g = tan(3.14159265358979323846 * cutoff / rate);
    double coef = g / (1.0 + g);
    double amplitudes[2];
    const unsigned frequencies[] = {1200, 2400};
    for (unsigned band = 0; band < 2; ++band) {
        assert(synth_init(&synth, rate));
        double energy = 0;
        for (unsigned i = 0; i < 4800; ++i) {
            double x = 0.4 * sin(6.28318530717958647692 * i * frequencies[band] / rate);
            double y = modules_filter(&synth.modules, x, coef, 0);
            if (i >= 2400) energy += y * y;
        }
        amplitudes[band] = sqrt(energy / 2400.0);
    }
    double octave_db = 20 * log10(amplitudes[1] / amplitudes[0]);
    assert(octave_db < -15.5 && octave_db > -19.0);

    assert(synth_init(&synth, rate));
    synth_clear_matrix(&synth);
    assert(synth_pin(&synth, SRC_FILTER, DST_OUT1_IN, PIN_FULL));
    synth_patch_controls(&synth, 0.0, 3.5, cutoff);
    synth_module_control(&synth, M4_RESPONSE, 1.0);
    for (unsigned i = 0; i < 8; ++i) synth_render(&synth, samples, FRAMES);
    assert(rms() > 0.006 && rising_crossings() >= 100 && rising_crossings() <= 106);
    double oscillation_hz = 10.0 * rising_crossings();
    synth_module_control(&synth, M4_RESPONSE, 0.0);
    for (unsigned i = 0; i < 3; ++i) synth_render(&synth, samples, FRAMES);
    assert(rms() < 0.0001);
    printf("Filtro a 48 kHz: %.2f dB/oitava entre 1,2 e 2,4 kHz, auto-oscilação %.0f Hz.\n",
           octave_db, oscillation_hz);
}

static void advance_envelope(Modules *m, unsigned frames)
{
    for (unsigned i = 0; i < frames; ++i) modules_envelope(m, 8000, 0, false, 0);
}

static void test_envelope_m4(void)
{
    Synth synth;
    assert(synth_init(&synth, 8000));
    Modules *m = &synth.modules;
    synth_module_control(&synth, M4_ENV_MODE, 1);
    synth_module_control(&synth, M4_ATTACK_MS, 10);
    synth_module_control(&synth, M4_ON_MS, 15);
    synth_module_control(&synth, M4_DECAY_MS, 20);
    synth_module_control(&synth, M4_OFF_MS, 15);
    synth_manual_attack(&synth);
    advance_envelope(m, 40);
    assert(m->phase == ENV_ATTACK && fabs(m->env - 0.5) < 1e-9);
    advance_envelope(m, 40);
    assert(m->phase == ENV_ON && m->env == 1);
    advance_envelope(m, 120);
    assert(m->phase == ENV_DECAY && m->env == 1);
    advance_envelope(m, 80);
    assert(fabs(m->env - 0.5) < 1e-9);
    advance_envelope(m, 80);
    assert(m->phase == ENV_OFF && m->env == 0 && !m->env_running);

    synth_manual_hold(&synth, true);
    advance_envelope(m, 800);
    assert(m->phase == ENV_ON && m->env == 1);
    synth_manual_hold(&synth, false);
    advance_envelope(m, 1);
    assert(m->phase == ENV_DECAY);
    advance_envelope(m, 250);

    synth_module_control(&synth, M4_ENV_MODE, 2);
    advance_envelope(m, 360);
    assert(m->phase == ENV_OFF && m->env_running);
    advance_envelope(m, 121);
    assert(m->phase == ENV_ATTACK && m->env > 0);

    assert(synth_init(&synth, 8000));
    synth_clear_matrix(&synth);
    synth_module_control(&synth, M4_TRIGGER_MODE, 1);
    synth_module_control(&synth, M4_ENV_MODE, 1);
    synth_module_control(&synth, M4_TRAP_LEVEL, 0.5);
    assert(synth_pin(&synth, SRC_INPUT1, DST_OUT2_IN, PIN_FULL));
    assert(synth_pin(&synth, SRC_TRAPEZOID, DST_METER, PIN_FULL));
    const float impulse[64] = {0.65f};
    synth_render_input(&synth, samples, NULL, 64, impulse, NULL);
    assert(synth.modules.phase == ENV_ON);
    assert(fabs(synth.meter_peak - 0.5) < 1e-12);

    assert(synth_init(&synth, 8000));
    synth_module_control(&synth, M4_ENV_MODE, 1);
    synth_module_control(&synth, M4_ATTACK_MS, 2);
    synth_module_control(&synth, M4_ON_MS, 0);
    synth_module_control(&synth, M4_DECAY_MS, 20);
    assert(synth_pin(&synth, SRC_INPUT2, DST_ENV_DECAY, PIN_FULL));
    float decay_cv[160];
    for (unsigned i = 0; i < 160; ++i) decay_cv[i] = 0.25f;
    synth_manual_attack(&synth);
    synth_render(&synth, samples, 16);
    synth_render_input(&synth, samples, NULL, 160, NULL, decay_cv);
    assert(synth.modules.phase == ENV_DECAY && fabs(synth.modules.env - 0.5) < 1e-9);
    synth_render_input(&synth, samples, NULL, 160, NULL, decay_cv);
    assert(synth.modules.phase == ENV_OFF);
}

static void test_spring_and_output_m4(void)
{
    Synth synth;
    assert(synth_init(&synth, 48000));
    synth_clear_matrix(&synth);
    assert(synth_pin(&synth, SRC_INPUT1, DST_REVERB_IN, PIN_FULL));
    assert(synth_pin(&synth, SRC_REVERB, DST_OUT1_IN, PIN_FULL));
    float impulse[FRAMES] = {1.0f};
    synth_render_input(&synth, samples, NULL, FRAMES, impulse, NULL);
    for (unsigned i = 0; i < 1200; ++i) assert(samples[i] == 0);
    assert(fabs(samples[1200]) > 0.01 && fabs(samples[1440]) > 0.01);
    assert(rms() > 0.0001);
    for (unsigned i = 0; i < 40; ++i) synth_render(&synth, samples, FRAMES);
    assert(rms() < 0.0001);

    assert(synth_init(&synth, 48000));
    synth_clear_matrix(&synth);
    assert(synth_pin(&synth, SRC_INPUT1, DST_REVERB_IN, PIN_FULL));
    assert(synth_pin(&synth, SRC_INPUT2, DST_REVERB_MIX, PIN_FULL));
    assert(synth_pin(&synth, SRC_REVERB, DST_OUT1_IN, PIN_FULL));
    const float wet_cv[] = {-1.0f};
    synth_render_input(&synth, samples, NULL, 1, impulse, wet_cv);
    assert(fabs(samples[0] - 0.0672) < 1e-7);

    assert(synth_init(&synth, 48000));
    synth_clear_matrix(&synth);
    assert(synth_pin(&synth, SRC_INPUT1, DST_OUT1_IN, PIN_FULL));
    synth_module_control(&synth, M4_IN1_LEVEL, 2);
    synth_module_control(&synth, M4_OUT1_LEVEL, 0.5);
    synth_module_control(&synth, M4_OUT1_PAN, 1);
    const float input[] = {0.25f, NAN, INFINITY, -INFINITY};
    float right[4];
    synth_render_input(&synth, samples, right, 4, input, NULL);
    assert(samples[0] == 0 && fabs(right[0] - 0.042) < 1e-7);
    for (unsigned i = 1; i < 4; ++i) assert(right[i] == 0 && samples[i] == 0);
}

static void test_limits_m4(void)
{
    const double rates[] = {8000, 48000, 192000};
    for (unsigned j = 0; j < sizeof rates / sizeof rates[0]; ++j) {
        Synth synth;
        assert(synth_init(&synth, rates[j]));
        synth_module_control(&synth, M4_RESPONSE, 1);
        synth_module_control(&synth, M4_FILTER_DRIVE, 4);
        synth_module_control(&synth, M4_REVERB_TIME, 4);
        synth_module_control(&synth, M4_NOISE_COLOR, 1);
        synth_module_control(&synth, M4_OSC2_PULSE, 1);
        synth_module_control(&synth, M4_OSC2_SHAPE, 0.05);
        synth_module_control(&synth, M4_ENV_MODE, 2);
        synth_patch_controls(&synth, 0.55, 500, 10000);
        synth_pin(&synth, SRC_FILTER, DST_FILTER_CUTOFF, PIN_FULL);
        synth_pin(&synth, SRC_NOISE, DST_FILTER_IN, PIN_FULL);
        synth_pin(&synth, SRC_FILTER, DST_REVERB_IN, PIN_FULL);
        synth_pin(&synth, SRC_REVERB, DST_REVERB_IN, PIN_FULL);
        synth_pin(&synth, SRC_REVERB, DST_OUT2_IN, PIN_FULL);
        for (unsigned block = 0; block < 12; ++block) {
            synth_render(&synth, samples, FRAMES);
            assert(isfinite(rms()));
        }
        assert(synth.modules.spring_length_a <= SPRING_A_MAX);
        assert(synth.modules.spring_length_b <= SPRING_B_MAX);
    }
}

static void test_odd_render_boundaries(void)
{
    enum { TEST_FRAMES = 129 };
    static Synth whole, split;
    float left[TEST_FRAMES], right[TEST_FRAMES], split_left[TEST_FRAMES], split_right[TEST_FRAMES];
    assert(synth_init(&whole, 44100));
    synth_repeat_patch(&whole);
    assert(synth_pin(&whole, SRC_REVERB, DST_REVERB_IN, PIN_HALF));
    synth_manual_attack(&whole);
    split = whole;
    synth_render_stereo(&whole, left, right, TEST_FRAMES);
    synth_render_stereo(&split, split_left, split_right, 1);
    synth_render_stereo(&split, split_left + 1, split_right + 1, 0);
    synth_render_stereo(&split, split_left + 1, split_right + 1, 65);
    synth_render_stereo(&split, split_left + 66, split_right + 66, 63);
    for (unsigned i = 0; i < TEST_FRAMES; ++i)
        assert(left[i] == split_left[i] && right[i] == split_right[i]);
    for (unsigned i = 0; i < MATRIX_SIZE; ++i)
        assert(whole.signals[i] == split.signals[i]);
}

static void test_flash_policy(void)
{
    assert(!flash_test_span(FLASH_UNVERIFIED, 0x9D000, 0x1000));
    assert(!flash_test_span((enum FlashTarget)99, 0x9D000, 0x1000));
    assert(!flash_test_span(FLASH_SMK37_PRO_REFERENCE, 0x9D000, 0));
    assert(!flash_test_span(FLASH_SMK37_PRO_REFERENCE, 0x9D001, 0x1000));
    assert(!flash_test_span(FLASH_SMK37_PRO_REFERENCE, 0xAE000, 0x2000));
    assert(!flash_test_span(FLASH_SMK37_PRO_REFERENCE, 0x9D000, UINT32_MAX));
    assert(flash_test_span(FLASH_SMK37_PRO_REFERENCE, 0x9D000, 0x12000));
    for (uint32_t address = 0; address < 0x100000; address += 0x1000) {
        bool inside = address >= 0x9D000 && address < 0xAF000;
        assert(flash_test_span(FLASH_SMK37_PRO_REFERENCE, address, 0x1000) == inside);
    }
}

static void test_performance(void)
{
    Synth synth;
    assert(synth_init(&synth, 8000));
    assert(synth_scale_note(61, 0, SCALE_MAJOR) == 60);
    assert(synth_scale_note(63, 0, SCALE_MINOR) == 63);
    assert(SCALE_COUNT == 32 && SCALE_LYDIAN == 7 && SCALE_IWATO == 31);
    assert(synth_scale_note(65, 0, SCALE_LYDIAN) == 64);
    assert(synth_scale_note(66, 0, SCALE_LYDIAN) == 66);
    assert(synth_scale_note(127, 0, SCALE_IWATO) == 126);
    synth_clear_matrix(&synth);
    assert(synth_pin(&synth, SRC_STICK_X, DST_OSC1_PITCH, PIN_HALF));
    synth_stick_x(&synth, 0.5);
    synth_render(&synth, samples, 1);
    assert(fabs(synth.hz[0] - 880.0) < 0.1);
    assert(synth_pin(&synth, SRC_STICK_Y, DST_OSC2_PITCH, PIN_HALF));
    assert(synth_midi(&synth, 0xb0, 17, 32));
    synth_render(&synth, samples, 1);
    assert(fabs(synth.hz[1] - 220.0) < 0.1);

    const KsTimedEvent octave[] = {{0, 81, 110}, {50, 81, 0}};
    assert(ks_load_ms(&synth.ks, octave, 2, 100));
    assert(ks_play(&synth.ks));
    assert(synth_ks_route(&synth, DST_OSC2_PITCH, PIN_FULL));
    synth_render(&synth, samples, 1);
    assert(fabs(synth.hz[1] - 440.0) < 0.1);
    synth_ks_matrix(&synth, true);
    synth_render(&synth, samples, 1);
    assert(fabs(synth.hz[1] - 622.254) < 0.1);
    synth_ks_matrix(&synth, false);
    synth_clear_matrix(&synth);
    synth_render(&synth, samples, 1);
    assert(fabs(synth.hz[1] - 880.0) < 0.1);

    assert(synth_init(&synth, 8000));
    synth_arp(&synth, true, 120, 50, ARP_UP, ARP_EIGHTH);
    synth_midi(&synth, 0x90, 72, 100);
    synth_midi(&synth, 0x90, 60, 100);
    synth_ks_record(&synth);
    synth_render(&synth, samples, 2010);
    assert(synth.ks.count == 3);
    assert(synth.ks.events[0].frame == 0 && synth.ks.events[0].note == 60);
    assert(synth.ks.events[1].frame == 1000 && synth.ks.events[1].velocity == 0);
    assert(synth.ks.events[2].frame == 2000 && synth.ks.events[2].note == 72);
    synth_arp(&synth, true, 120, 50, ARP_DOWN, ARP_EIGHTH);
    synth_render(&synth, samples, 1);
    assert(synth.arp.note == 72);
    synth_arp(&synth, true, 120, 50, ARP_PLAYED, ARP_EIGHTH);
    synth_render(&synth, samples, 1);
    assert(synth.arp.note == 72);
    synth_all_notes_off(&synth);
    synth_render(&synth, samples, 1);
    assert(synth.arp.velocity == 0);
    synth_arp(&synth, false, 120, 50, ARP_UP, ARP_EIGHTH);
    synth_midi(&synth, 0x90, 60, 100);
    synth_render(&synth, samples, 1);
    const unsigned periods[] = {4000, 2667, 2000, 1333, 1000, 667, 500, 333};
    for (unsigned i = 0; i < ARP_DIVISION_COUNT; ++i) {
        synth_arp(&synth, false, 120, 50, ARP_UP, (enum ArpDivision)i);
        assert(synth.arp.period == periods[i] && synth.arp.gate_frames == periods[i] / 2);
        assert(!synth.live_attack);
    }
}

static void test_step_sequencer(void)
{
    Synth synth;
    assert(synth_init(&synth, 8000));
    assert(synth.sequencer.length == 16);
    synth_seq_timing(&synth, 2, 120);
    assert(synth.sequencer.period == 1000);
    assert(synth_seq_set(&synth, 0, 69, 105));
    assert(synth_seq_set(&synth, 1, 81, 107));
    assert(synth_seq_set(&synth, 17, 76, 90));
    assert(!synth_seq_set(&synth, 64, 76, 90));
    const KsTimedEvent phrase[] = {{0, 60, 100}, {50, 60, 0}};
    assert(ks_load_ms(&synth.ks, phrase, 2, 100));
    assert(ks_play(&synth.ks));
    synth.ks.realtime_spread = 50;
    synth.ks.transpose = 12;
    synth_seq_play(&synth, true);
    assert(synth.ks.mode == KS_STOPPED);
    assert(synth.ks.count == 2);
    synth_render(&synth, samples, 750);
    assert(synth.sequencer.velocity == 105);
    assert(fabs(synth.hz[0] - 440.0) < 0.1);
    synth_render(&synth, samples, 250);
    assert(synth.sequencer.velocity == 0);
    assert(synth_pin(&synth, SRC_INPUT1, DST_OSC1_PITCH, PIN_OFF));
    assert(synth_ks_route(&synth, DST_OSC1_PITCH, PIN_FULL));
    synth_render(&synth, samples, 1);
    assert(synth.sequencer.index == 1 && synth.sequencer.velocity == 107);
    assert(fabs(synth.hz[0] - 880.0) < 0.1);
    synth_seq_timing(&synth, 1, 120);
    synth_render(&synth, samples, 1);
    assert(synth.sequencer.index == 0 && synth.sequencer.velocity == 105);
    synth_seq_timing(&synth, 18, 120);
    assert(synth.sequencer.steps[17].note == 76 && synth.sequencer.steps[17].velocity == 90);
    synth_ks_record(&synth);
    assert(!synth.sequencer.playing && synth.ks.mode == KS_RECORDING);
}

static void test_performance_integration(void)
{
    Synth synth;
    assert(synth_init(&synth, 8000));
    const KsTimedState phrase[] = {
        {31, 60, 92, true}, {153, 64, 85, false}, {210, 64, 0, false},
        {261, 67, 101, true}, {319, 67, 0, false},
    };
    assert(ks_load_states(&synth.ks, phrase, 5, 387));
    assert(synth_seq_from_ks(&synth));
    StepSequencer *seq = &synth.sequencer;
    assert(seq->length == 3 && seq->steps[1].legato);
    const uint32_t length = synth.ks.length;
    assert(synth_seq_to_ks(&synth));
    assert(synth.ks.length == length && synth.ks.count == 5);
    for (unsigned i = 0; i < 5; ++i) {
        assert(synth.ks.events[i].frame == phrase[i].ms * 8u);
        assert(synth.ks.events[i].note == phrase[i].note);
        assert(synth.ks.events[i].velocity == phrase[i].velocity);
        assert(synth.ks.events[i].attack == phrase[i].attack);
    }

    float pcm[4000];
    synth_seq_play(&synth, true);
    synth_render(&synth, pcm, 31 * 8);
    assert(seq->velocity == 0);
    synth_render(&synth, pcm, (153 - 31) * 8);
    assert(seq->velocity == 92 && seq->note == 60);
    synth_render(&synth, pcm, 1);
    assert(seq->velocity == 85 && seq->note == 64);
    synth_seq_play(&synth, false);

    synth_arp_scale(&synth, 0, SCALE_MAJOR);
    synth_arp(&synth, true, 120, 50, ARP_UP, ARP_SIXTEENTH_TRIPLET);
    synth_arp_modes(&synth, true, false, 100);
    synth_midi(&synth, 0x90, 61, 93);
    synth_midi(&synth, 0x80, 61, 0);
    synth_render(&synth, pcm, 1);
    assert(synth.arp.note == 60 && synth.arp.velocity == 93);
    synth_midi(&synth, 0x90, 66, 87);
    synth_midi(&synth, 0x80, 66, 0);
    synth_render(&synth, pcm, synth.arp.period);
    assert(synth.arp.note == 65 && synth.arp.velocity == 87);

    synth_arp_modes(&synth, false, true, 0);
    synth_seq_timing(&synth, 3, 122);
    synth_seq_record(&synth);
    assert(seq->recording && seq->period == synth.arp.period && seq->bpm == 120);
    synth_render(&synth, pcm, seq->period);
    assert(seq->steps[0].velocity == 0);
    synth_arp_modes(&synth, false, true, 100);
    synth_render(&synth, pcm, 2 * seq->period);
    assert(!seq->recording && seq->playing);
    assert(seq->steps[1].velocity == 100 && seq->steps[2].velocity == 100);
    assert(seq->steps[1].duration == synth.arp.gate_frames);
    assert(synth_scale_note(seq->steps[1].note, 0, SCALE_MAJOR) == seq->steps[1].note);
    assert(synth_seq_to_ks(&synth));
    assert(synth.ks.events[0].frame == seq->period);

    synth_seq_play(&synth, false);
    synth_all_notes_off(&synth);
    synth_render(&synth, pcm, 1);
    assert(!synth.arp.free_run && !synth.arp.latched_count && !synth.arp.velocity);
    synth_arp_scale(&synth, 2, SCALE_MAJOR);
    synth_arp_modes(&synth, false, true, 100);
    synth_render(&synth, pcm, 1);
    assert(synth.arp.note == 62 && synth.arp.velocity == 100);
}

static void test_aks_calibration(void)
{
    Synth synth;
    const ZiviReal rate = 8000;
    assert(synth_init(&synth, rate));
    Modules *m = &synth.modules;
    modules_set_profile(m, rate, true);
    modules_set(m, rate, M4_OUT1_LEVEL, 0);
    assert(modules_output(m, 0, 0.5, 0) == 0);
    assert(modules_output(m, 0, 0.5, 0.5) == 0.25);
    modules_set(m, rate, M4_OUT2_LEVEL, 0.5);
    assert(modules_output(m, 1, 0.5, -0.5) == 0);

    modules_set(m, rate, M4_ENV_MODE, 2);
    modules_set(m, rate, M4_OFF_MS, 5000);
    modules_set(m, rate, M4_ATTACK_MS, 2);
    modules_set(m, rate, M4_ON_MS, 2500);
    modules_set(m, rate, M4_DECAY_MS, 8);
    advance_envelope(m, 16);
    assert(m->phase == ENV_ON);
    advance_envelope(m, 39999);
    assert(m->phase == ENV_ON);
    advance_envelope(m, 1);
    assert(m->phase == ENV_DECAY);
    advance_envelope(m, 64 + 6 * rate);
    assert(m->phase == ENV_OFF && !m->env_running && m->env == 0);
    modules_envelope(m, rate, 1, true, 0.3125);
    assert(fabs(m->decay_frames - 128) < 1e-8);
    advance_envelope(m, 15 + 19999);
    assert(m->phase == ENV_ON);
    advance_envelope(m, 1);
    assert(m->phase == ENV_DECAY);

    modules_set(m, rate, M4_REVERB_LEVEL, 1);
    modules_set(m, rate, M4_REVERB_MIX, 0.2);
    assert(fabs(modules_reverb(m, 0.5, 0) - 0.5) < 1e-8);
    modules_set(m, rate, M4_REVERB_TIME, 2);
    ZiviReal feedback = m->spring_feedback_a;
    modules_set(m, rate, M4_REVERB_TIME, 4);
    assert(m->spring_feedback_a == feedback);
    modules_set_profile(m, rate, false);
    assert(m->spring_feedback_a > feedback);
    assert(modules_output(m, 0, 0.5, 0.5) == 0);

    assert(synth_init(&synth, 44118));
    modules_set_profile(&synth.modules, synth.rate, true);
    synth_clear_matrix(&synth);
    synth_module_control(&synth, M4_OSC1_HZ, 10000);
    synth_module_control(&synth, M4_OSC2_HZ, 10000);
    synth_render(&synth, samples, 4);
    assert(fabs(synth.hz[0] - 10000) < 1e-6 && fabs(synth.hz[1] - 10000) < 1e-6);
    puts("AKS: CV de saída, Off sem repetição, On automático/manual, Decay, mola e 10 kHz verificados.");
}

int main(void)
{
    test_performance();
    test_step_sequencer();
    test_performance_integration();
    test_matrix();
    test_chains_and_feedback();
    test_midi();
    test_transport();
    test_ks_performance();
    test_filter_m4();
    test_envelope_m4();
    test_spring_and_output_m4();
    test_aks_calibration();
    test_limits_m4();
    test_odd_render_boundaries();
    test_flash_policy();
    puts("C: matriz, estéreo, filtro, anel, realimentação, MIDI, KS, módulos M4 e política de flash aprovados.");
    return 0;
}
