#include "../platform/smk37_fm1_base/master.h"
#include <assert.h>
#include <stdlib.h>

static int32_t pcm[2 * MASTER_FRAMES];
static MasterLimiter limiter = {.gain = UNITY_Q12, .limit = UNITY_Q12};

static void block(int32_t left, int32_t right)
{
    for (unsigned i = 0; i < MASTER_FRAMES; ++i) {
        pcm[2 * i] = left;
        pcm[2 * i + 1] = right;
    }
    master_limit_block(&limiter, pcm);
    for (unsigned i = 0; i < 2 * MASTER_FRAMES; ++i)
        assert(pcm[i] >= -CEILING_Q23 && pcm[i] <= CEILING_Q23);
}

int main(void)
{
    assert(q12_scale(-12345, UNITY_Q12) == -12345);
    assert(q12_scale(-12345, 2048) == -6173);
    block(1000000, -500000);
    assert(pcm[0] == 0);
    block(1000000, -500000);
    assert(pcm[0] == 1000000 && pcm[255] == -500000);
    block(16000000, -8000000);
    assert(pcm[0] == 1000000 && pcm[254] < 500000);
    block(1000000, -500000);
    assert(pcm[0] > 7400000 && abs(pcm[0] + 2 * pcm[1]) < 3);
    assert(pcm[254] == pcm[0]);
    block(1000000, -500000);
    assert(pcm[0] < 500000 && pcm[254] > pcm[0]);
    for (unsigned i = 0; i < 128; ++i) block(1000000, -500000);
    assert(pcm[0] == 1000000 && pcm[255] == -500000);
}
