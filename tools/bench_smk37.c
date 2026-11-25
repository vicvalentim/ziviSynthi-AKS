#include "zivi_smk37.h"

#include <stdio.h>
#include <time.h>

static ZiviSmk37 engine;

int main(void)
{
    enum { BLOCKS = 8192 };
    printf("Host x86-64, 44,1 kHz, %d blocos de %d quadros por preset.\n", BLOCKS, ZIVI_SMK_FRAMES);
    for (unsigned preset = 0; preset < synth_factory_count(); ++preset) {
        zivi_smk_init(&engine);
        if (!synth_factory_apply(&engine.synth, preset)) return 1;
        if (engine.synth.ks.sequence_trigger) ks_play(&engine.synth.ks);
        else zivi_smk_key(&engine, 12, 112, 0);
        int32_t pcm[ZIVI_SMK_WORDS];
        int64_t checksum = 0;
        clock_t start = clock();
        for (unsigned block = 0; block < BLOCKS; ++block) {
            if (!zivi_smk_fill(&engine) || !zivi_smk_irq_read(&engine, pcm)) return 1;
            checksum += pcm[(block * 17) % ZIVI_SMK_WORDS];
        }
        double time = (double)(clock() - start) / CLOCKS_PER_SEC;
        printf("%-16s %6.2f us/bloco  %5.2f ns/quadro  underruns %u  soma %lld\n",
               synth_factory_display(preset), time * 1e6 / BLOCKS, time * 1e9 / (BLOCKS * ZIVI_SMK_FRAMES),
               engine.underruns, (long long)checksum);
    }
    return 0;
}
