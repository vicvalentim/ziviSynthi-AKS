#include "zivi_smk37.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    enum { RELEASE_BLOCK = 512, BLOCKS = 1536 };
    static ZiviSmk37 engine;
    int32_t pcm[ZIVI_SMK_WORDS];
    puts("index,display,q23_fnv1a64,peak_q23,rms_l,rms_r,dc_l,dc_r,clip_count,last_tail_ms,ks_events");
    for (unsigned p = 0; p < synth_factory_count(); ++p) {
        zivi_smk_init(&engine);
        if (!synth_factory_apply(&engine.synth, p)) return EXIT_FAILURE;
        if (engine.synth.ks.sequence_trigger) {
            if (!ks_play(&engine.synth.ks)) return EXIT_FAILURE;
        } else if (!zivi_smk_key(&engine, 12, 112, 0)) return EXIT_FAILURE;
        uint64_t code = UINT64_C(14695981039346656037);
        unsigned peak = 0, clips = 0, last_tail = 0;
        double sum[2] = {0}, square[2] = {0};
        for (unsigned b = 0; b < BLOCKS; ++b) {
            if (b == RELEASE_BLOCK) {
                ks_stop(&engine.synth.ks);
                zivi_smk_key(&engine, 12, 0, 0);
            }
            if (!zivi_smk_fill(&engine) || !zivi_smk_irq_read(&engine, pcm)) return EXIT_FAILURE;
            for (unsigned i = 0; i < ZIVI_SMK_WORDS; ++i) {
                int32_t sample = pcm[i];
                if (sample < -8388608 || sample > 8388607) return EXIT_FAILURE;
                for (unsigned shift = 0; shift < 24; shift += 8) {
                    code ^= ((uint32_t)sample >> shift) & 255u;
                    code *= UINT64_C(1099511628211);
                }
                unsigned magnitude = (unsigned)(sample < 0 ? -sample : sample), ch = i & 1;
                if (magnitude > peak) peak = magnitude;
                if (b >= RELEASE_BLOCK && magnitude > 8389) last_tail = (b - RELEASE_BLOCK) * ZIVI_SMK_FRAMES + i / 2;
                clips += sample == 8388607 || sample == -8388608;
                double fraction = sample / 8388608.0;
                square[ch] += fraction * fraction;
                sum[ch] += fraction;
            }
        }
        const double frames = BLOCKS * ZIVI_SMK_FRAMES;
        printf("%u,\"%s\",%016" PRIx64 ",%u,%.9f,%.9f,%.9f,%.9f,%u,%u,%u\n",
               p, synth_factory_display(p), code, peak, sqrt(square[0] / frames), sqrt(square[1] / frames),
               sum[0] / frames, sum[1] / frames, clips, last_tail * 1000 / ZIVI_SMK_RATE,
               (unsigned)engine.synth.ks.count);
    }
}
