#include "zivi_smk37.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static ZiviSmk37 engine;

static int compare(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

int main(void)
{
    enum { BLOCKS = 4800 };
    static double times[BLOCKS];
    int32_t pcm[ZIVI_SMK_WORDS];
    zivi_smk_init(&engine);
    if (!synth_factory_apply(&engine.synth, 46) || !ks_play(&engine.synth.ks)) return EXIT_FAILURE;
    for (unsigned r = 0; r < MATRIX_SIZE; ++r)
        for (unsigned c = 0; c < MATRIX_SIZE; ++c)
            if (synth_pin_supported(r, c) && !synth_pin(&engine.synth, r, c, PIN_HALF)) return EXIT_FAILURE;
    synth_module_control(&engine.synth, M4_RESPONSE, 0.82);
    synth_module_control(&engine.synth, M4_REVERB_MIX, 0.8);
    for (unsigned i = 0; i < ZIVI_SMK_SLOTS - 1; ++i)
        if (!zivi_smk_fill(&engine)) return EXIT_FAILURE;
    double total = 0;
    for (unsigned i = 0; i < BLOCKS; ++i) {
        clock_t start = clock();
        if (i % 13 == 0) zivi_smk_midi_port(&engine, 1, 0x90, 32 + (i % 60), 100);
        if (i % 13 == 7) zivi_smk_midi_port(&engine, 1, 0xb0, 123, 0);
        if (i % 19 == 0) synth_toggle_pin(&engine.synth, SRC_REVERB, DST_REVERB_IN, PIN_HALF);
        if (!zivi_smk_irq_read(&engine, pcm) || !zivi_smk_fill(&engine)) return EXIT_FAILURE;
        for (unsigned j = 0; j < ZIVI_SMK_WORDS; ++j)
            if (pcm[j] < -8388608 || pcm[j] > 8388607) return EXIT_FAILURE;
        if (!isfinite(engine.synth.modules.env) || !isfinite(engine.synth.meter_peak)) return EXIT_FAILURE;
        times[i] = (double)(clock() - start) * 1e6 / CLOCKS_PER_SEC;
        total += times[i];
    }
    qsort(times, BLOCKS, sizeof times[0], compare);
    printf("Host Q23, %d blocos, matriz densa + feedback, KS, MIDI e edição de pinos.\n", BLOCKS);
    printf("us/bloco (CPU host) média %.2f p95 %.2f p99 %.2f máximo %.2f; underruns %u.\n",
           total / BLOCKS, times[BLOCKS * 95 / 100 - 1], times[BLOCKS * 99 / 100 - 1],
           times[BLOCKS - 1], engine.underruns);
    return engine.underruns ? EXIT_FAILURE : EXIT_SUCCESS;
}
