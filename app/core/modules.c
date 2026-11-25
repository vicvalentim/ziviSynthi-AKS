#include "modules.h"

#include "dsp_curve.h"
#include "aks_calibration.h"

#define TAU 6.28318530717958647692

const ModuleParamInfo module_params[M4_COUNT] = {
#define PARAM(id, js, group, label, lo, hi, initial, step) [M4_##id] = {group, label, lo, hi, initial, step},
#include "parameters.def"
#undef PARAM
};

static ZiviReal clamp_value(ZiviReal value, ZiviReal lo, ZiviReal hi)
{
    return dsp_min(hi, dsp_max(lo, value));
}

void modules_set(Modules *m, ZiviReal rate, unsigned param, ZiviReal value)
{
    if (param >= M4_COUNT || !isfinite(value)) return;
    const ModuleParamInfo *info = &module_params[param];
    value = clamp_value(value, info->lo, info->hi);
    if (param == M4_ENV_MODE || param == M4_TRIGGER_MODE) value = (unsigned)(value + 0.5);
    if (param == M4_ENV_MODE) {
        if (m->p[param] != value) {
            m->phase = ENV_OFF;
            m->env_elapsed = m->retrigger = 0;
            m->env_running = m->held_gate = false;
            m->manual_attack = value == 2.0;
            if (value != 0.0) m->env = 0.0;
        }
    }
    m->p[param] = value;
    if (param == M4_ATTACK_MS) m->attack_coeff = 1.0 - dsp_exp(-1000.0 / (rate * value));
    if (param == M4_DECAY_MS) m->decay_mod_last = 2.0;
    if (param == M4_REVERB_TIME) {
        if (m->aks_profile) value = dsp_min(value, aks_reverb_max_seconds);
        m->spring_feedback_a = dsp_exp(-6.907755278982137 * m->spring_length_a / (rate * value));
        m->spring_feedback_b = dsp_exp(-6.907755278982137 * m->spring_length_b / (rate * value));
    }
}

void modules_set_profile(Modules *m, ZiviReal rate, bool aks)
{
    if (m->aks_profile == aks) return;
    m->aks_profile = aks;
    m->decay_mod_last = 2.0;
    modules_set(m, rate, M4_REVERB_TIME, m->p[M4_REVERB_TIME]);
}

void modules_defaults(Modules *m, ZiviReal rate)
{
    for (unsigned i = 0; i < M4_COUNT; ++i) modules_set(m, rate, i, module_params[i].initial);
}

void modules_init(Modules *m, ZiviReal rate)
{
    m->spring_length_a = (unsigned)(rate * 0.025 + 0.5);
    m->spring_length_b = (unsigned)(rate * 0.030 + 0.5);
    m->spring_damping = 1.0 - dsp_exp(-TAU * 4200.0 / rate);
    m->tone_coeff = m->noise_coeff = 1.0 - dsp_exp(-TAU * 1100.0 / rate);
    m->retrigger_coeff = 1.0 - dsp_exp(-2000.0 / rate);
    m->retrigger_length = (unsigned)(rate * 0.002);
    modules_defaults(m, rate);
}

ZiviReal modules_envelope(Modules *m, ZiviReal rate, ZiviReal gate, bool trigger, ZiviReal decay_cv)
{
    if (decay_cv != m->decay_mod_last) {
        ZiviReal sensitivity = m->aks_profile ? aks_cv_reference / aks_decay_octave : 4.0;
        ZiviReal milliseconds = clamp_value(m->p[M4_DECAY_MS] * dsp_exp2(sensitivity * decay_cv), 3.0, 15000.0);
        m->decay_frames = rate * 0.001 * milliseconds;
        m->decay_coeff = dsp_decay_coeff(m->decay_frames);
        m->decay_mod_last = decay_cv;
    }

    if (m->p[M4_ENV_MODE] == 0.0) {
        if (m->manual_attack) {
            m->manual_attack = false;
            m->env_elapsed = (unsigned)(rate * 0.001 * (m->p[M4_ATTACK_MS] + m->p[M4_ON_MS]));
            trigger = true;
        }
        if (m->env_elapsed || m->manual_hold) {
            if (m->env_elapsed) --m->env_elapsed;
            gate = 1.0;
        }
        if (gate == 0.0) m->retrigger = 0;
        else if (trigger && m->env > 0.02) m->retrigger = m->retrigger_length;
        if (m->retrigger) {
            --m->retrigger;
            m->env -= m->retrigger_coeff * m->env;
        } else {
            ZiviReal coefficient = gate > m->env ? m->attack_coeff : m->decay_coeff;
            m->env += coefficient * (gate - m->env);
        }
        m->phase = gate > 0.0 ? (m->env + 0.01 < gate ? ENV_ATTACK : ENV_ON)
                             : (m->env > 1e-8 ? ENV_DECAY : ENV_OFF);
        return m->env;
    }

    bool repeat = m->p[M4_ENV_MODE] == 2.0 && (!m->aks_profile || m->p[M4_OFF_MS] < 5000.0);
    if (m->manual_attack || trigger || (gate > 0.0 && !m->held_gate)) {
        m->manual_cycle = m->manual_attack;
        m->env_peak = m->manual_attack ? 1.0 : gate;
        m->manual_attack = false;
        m->env_running = true;
        m->env_start = m->env;
        m->env_elapsed = 0;
        m->phase = ENV_ATTACK;
    }
    m->held_gate = gate > 0.0;
    if (!m->env_running) return 0.0;

    unsigned elapsed = ++m->env_elapsed;
    if (m->phase == ENV_ATTACK) {
        ZiviReal position = clamp_value(elapsed / (rate * 0.001 * m->p[M4_ATTACK_MS]), 0.0, 1.0);
        m->env = m->env_start + (m->env_peak - m->env_start) * position;
        if (position >= 1.0) {
            m->phase = (m->p[M4_ON_MS] > 0.0 || (m->manual_cycle && m->manual_hold)) ? ENV_ON : ENV_DECAY;
            m->env_elapsed = 0;
        }
    } else if (m->phase == ENV_ON) {
        ZiviReal on = m->p[M4_ON_MS];
        if (m->aks_profile && m->manual_cycle) on *= aks_manual_on_ratio;
        if (elapsed >= rate * 0.001 * on && !(m->manual_cycle && m->manual_hold)) {
            m->phase = ENV_DECAY;
            m->env_elapsed = 0;
        }
    } else if (m->phase == ENV_DECAY) {
        m->env = m->env_peak * dsp_max(0.0, 1.0 - elapsed / m->decay_frames);
        if (m->env == 0.0) {
            m->phase = ENV_OFF;
            m->env_elapsed = 0;
            m->env_running = repeat;
            m->manual_cycle = false;
        }
    } else if (!repeat && m->aks_profile) {
        m->env_running = false;
    } else if (elapsed >= rate * 0.001 * m->p[M4_OFF_MS]) {
        m->env_start = 0.0;
        m->env_elapsed = 0;
        m->phase = ENV_ATTACK;
    }
    return m->env;
}

ZiviReal modules_filter(Modules *m, ZiviReal input, ZiviReal g, ZiviReal seed)
{
    ZiviReal current = 1.0 - g;
    ZiviReal feedback = 9.1 * m->p[M4_RESPONSE];
    ZiviReal state = current * (m->filter_stage[2] + g * (m->filter_stage[1] + g * m->filter_stage[0]));
    ZiviReal signal = input * m->p[M4_FILTER_DRIVE];
    if (feedback > 8.0) signal += seed * 1e-7;
    ZiviReal y = clamp_value((signal - feedback * state) / (1.0 + feedback * g * g * g), -1.0, 1.0);
    for (unsigned pole = 0; pole < 3; ++pole) {
        ZiviReal delta = (y - m->filter_stage[pole]) * g;
        y = m->filter_stage[pole] + delta;
        m->filter_stage[pole] = y + delta;
    }
    return clamp_value(y, -1.0, 1.0) * m->p[M4_FILTER_LEVEL];
}

ZiviReal modules_reverb(Modules *m, ZiviReal input, ZiviReal mix_cv)
{
    const ZiviReal dispersion = 0.57;
    ZiviReal a = m->spring_a[m->spring_index_a];
    ZiviReal b = m->spring_b[m->spring_index_b];
    ZiviReal diffuse_a = m->spring_diffuse_a - dispersion * a;
    ZiviReal diffuse_b = m->spring_diffuse_b + dispersion * b;
    m->spring_diffuse_a = a + dispersion * diffuse_a;
    m->spring_diffuse_b = b - dispersion * diffuse_b;
    m->spring_low_a += m->spring_damping * (diffuse_a - m->spring_low_a);
    m->spring_low_b += m->spring_damping * (diffuse_b - m->spring_low_b);
    m->spring_a[m->spring_index_a] = (float)clamp_value(0.45 * input + m->spring_feedback_a * m->spring_low_a, -1.0, 1.0);
    m->spring_b[m->spring_index_b] = (float)clamp_value(0.45 * input - m->spring_feedback_b * m->spring_low_b, -1.0, 1.0);
    if (++m->spring_index_a == m->spring_length_a) m->spring_index_a = 0;
    if (++m->spring_index_b == m->spring_length_b) m->spring_index_b = 0;
    ZiviReal mix = m->p[M4_REVERB_MIX];
    if (m->aks_profile)
        mix = dsp_max(0.0, mix - aks_reverb_mix_start) / (1.0 - aks_reverb_mix_start) +
              (aks_cv_reference / aks_reverb_cv_span) * mix_cv;
    else mix += 0.5 * mix_cv;
    ZiviReal wet = clamp_value(mix, 0.0, 1.0);
    ZiviReal echo = 0.5 * (diffuse_a + diffuse_b);
    return ((1.0 - wet) * input + wet * echo) * m->p[M4_REVERB_LEVEL];
}

ZiviReal modules_output(Modules *m, unsigned channel, ZiviReal input, ZiviReal modulation)
{
    static const unsigned level[] = {M4_OUT1_LEVEL, M4_OUT2_LEVEL};
    static const unsigned tone[] = {M4_OUT1_TONE, M4_OUT2_TONE};
    ZiviReal amount = m->p[tone[channel]];
    m->tone_low[channel] += m->tone_coeff * (input - m->tone_low[channel]);
    ZiviReal low = m->tone_low[channel];
    ZiviReal coloured = input - dsp_max(0.0, amount) * low + dsp_min(0.0, amount) * (input - low);
    const ZiviReal signal = clamp_value(coloured, -1.0, 1.0);
    const ZiviReal gain = m->p[level[channel]];
    if (m->aks_profile) return signal * clamp_value(gain + modulation, 0.0, 1.0);
    return signal * modulation * gain;
}
