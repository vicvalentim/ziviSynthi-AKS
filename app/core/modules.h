#ifndef ZIVI_MODULES_H
#define ZIVI_MODULES_H

#include <stdbool.h>
#include "dsp_real.h"

enum ModuleParam {
#define PARAM(id, js, group, label, lo, hi, initial, step) M4_##id,
#include "parameters.def"
#undef PARAM
    M4_COUNT
};

typedef struct {
    unsigned group;
    const char *label;
    ZiviReal lo, hi, initial, step;
} ModuleParamInfo;

enum EnvelopePhase { ENV_OFF, ENV_ATTACK, ENV_ON, ENV_DECAY };
#ifndef ZIVI_MAX_RATE
#define ZIVI_MAX_RATE 192000u
#endif
enum {
    SPRING_A_MAX = (ZIVI_MAX_RATE * 25u + 500u) / 1000u,
    SPRING_B_MAX = (ZIVI_MAX_RATE * 30u + 500u) / 1000u
};

typedef struct {
    ZiviReal p[M4_COUNT];
    ZiviReal attack_coeff, decay_coeff, decay_mod_last, decay_frames, retrigger_coeff;
    ZiviReal noise_low, noise_coeff;
    ZiviReal filter_stage[3];
    ZiviReal spring_feedback_a, spring_feedback_b;
    ZiviReal spring_low_a, spring_low_b, spring_diffuse_a, spring_diffuse_b, spring_damping;
    ZiviReal tone_low[2], tone_coeff;
    ZiviReal env, env_peak, env_start;
    unsigned spring_index_a, spring_index_b, spring_length_a, spring_length_b;
    unsigned env_elapsed, retrigger, retrigger_length;
    enum EnvelopePhase phase;
    bool env_running, held_gate;
    bool manual_attack, manual_hold, manual_cycle;
    bool aks_profile;
    float spring_a[SPRING_A_MAX], spring_b[SPRING_B_MAX];
} Modules;

extern const ModuleParamInfo module_params[M4_COUNT];

void modules_init(Modules *m, ZiviReal rate);
void modules_defaults(Modules *m, ZiviReal rate);
void modules_set(Modules *m, ZiviReal rate, unsigned param, ZiviReal value);
void modules_set_profile(Modules *m, ZiviReal rate, bool aks);
ZiviReal modules_envelope(Modules *m, ZiviReal rate, ZiviReal gate, bool trigger, ZiviReal decay_cv);
ZiviReal modules_filter(Modules *m, ZiviReal input, ZiviReal coefficient, ZiviReal seed);
ZiviReal modules_reverb(Modules *m, ZiviReal input, ZiviReal mix_cv);
ZiviReal modules_output(Modules *m, unsigned channel, ZiviReal input, ZiviReal modulation);

#endif
