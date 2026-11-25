/* Compartilha a tabela de parâmetros com o núcleo C. */
const ModuleDSP = (() => {
    const tau = 2 * Math.PI;
    const clamp = (x, lo, hi) => Math.min(hi, Math.max(lo, x));
    const defaults = Object.fromEntries(ModuleCatalog.map(({ id, initial }) => [id, initial]));

    class Modules {
        constructor(rate) {
            this.rate = rate;
            this.aksProfile = false;
            this.p = {};
            this.springA = new Float32Array(Math.round(rate * 0.025));
            this.springB = new Float32Array(Math.round(rate * 0.030));
            this.springFeedbackA = this.springFeedbackB = 0;
            this.springLowA = this.springLowB = this.springDiffuseA = this.springDiffuseB = 0;
            this.springIndexA = this.springIndexB = 0;
            this.springDamping = 1 - Math.exp(-tau * 4200 / rate);
            this.noiseCoeff = this.toneCoeff = 1 - Math.exp(-tau * 1100 / rate);
            this.noiseLow = 0;
            this.toneLow = [0, 0];
            this.filterStage = [0, 0, 0];
            this.phase = 0;
            this.env = this.envPeak = this.envStart = this.envElapsed = this.retrigger = 0;
            this.envRunning = this.heldGate = this.manualAttack = this.manualHold = this.manualCycle = false;
            this.retriggerCoeff = 1 - Math.exp(-2000 / rate);
            this.retriggerLength = Math.trunc(rate * 0.002);
            this.setControls(defaults);
        }

        setControls(values) {
            for (const { id, lo, hi } of ModuleCatalog) {
                if (!Number.isFinite(values[id])) continue;
                let value = clamp(values[id], lo, hi);
                if (id === "envMode" || id === "triggerMode") value = Math.round(value);
                if (this.p[id] === value) continue;
                this.p[id] = value;
                if (id === "envMode") {
                    this.phase = this.envElapsed = this.retrigger = 0;
                    this.envRunning = this.heldGate = false;
                    this.manualAttack = value === 2;
                    if (value !== 0) this.env = 0;
                } else if (id === "attackMs") {
                    this.attackCoeff = 1 - Math.exp(-1000 / (this.rate * value));
                } else if (id === "decayMs") {
                    this.decayModLast = 2;
                } else if (id === "reverbTime") {
                    this.setSpringTime();
                }
            }
        }

        setSpringTime() {
            const value = this.aksProfile ? Math.min(this.p.reverbTime, AKSCalibration.reverb_max_seconds) : this.p.reverbTime;
            const k = -6.907755278982137 / (this.rate * value);
            this.springFeedbackA = Math.exp(k * this.springA.length);
            this.springFeedbackB = Math.exp(k * this.springB.length);
        }

        setProfile(aks) {
            if (this.aksProfile === aks) return;
            this.aksProfile = aks;
            this.decayModLast = 2;
            this.setSpringTime();
        }

        envelope(gate, trigger, decayCv) {
            const p = this.p;
            const rate = this.rate;
            if (decayCv !== this.decayModLast) {
                const sensitivity = this.aksProfile ? AKSCalibration.cv_reference / AKSCalibration.decay_octave : 4;
                const milliseconds = clamp(p.decayMs * 2 ** (sensitivity * decayCv), 3, 15000);
                this.decayFrames = rate * 0.001 * milliseconds;
                this.decayCoeff = 1 - Math.exp(-1 / this.decayFrames);
                this.decayModLast = decayCv;
            }
            if (p.envMode === 0) {
                if (this.manualAttack) {
                    this.manualAttack = false;
                    this.envElapsed = Math.trunc(rate * 0.001 * (p.attackMs + p.onMs));
                    trigger = true;
                }
                if (this.envElapsed || this.manualHold) {
                    if (this.envElapsed) this.envElapsed--;
                    gate = 1;
                }
                if (gate === 0) this.retrigger = 0;
                else if (trigger && this.env > 0.02) this.retrigger = this.retriggerLength;
                if (this.retrigger) {
                    this.retrigger--;
                    this.env -= this.retriggerCoeff * this.env;
                } else {
                    const coefficient = gate > this.env ? this.attackCoeff : this.decayCoeff;
                    this.env += coefficient * (gate - this.env);
                }
                this.phase = gate > 0 ? (this.env + 0.01 < gate ? 1 : 2) : (this.env > 1e-8 ? 3 : 0);
                return this.env;
            }

            const repeat = p.envMode === 2 && (!this.aksProfile || p.offMs < 5000);
            if (this.manualAttack || trigger || (gate > 0 && !this.heldGate)) {
                this.manualCycle = this.manualAttack;
                this.envPeak = this.manualAttack ? 1 : gate;
                this.manualAttack = false;
                this.envRunning = true;
                this.envStart = this.env;
                this.envElapsed = 0;
                this.phase = 1;
            }
            this.heldGate = gate > 0;
            if (!this.envRunning) return 0;

            const elapsed = ++this.envElapsed;
            if (this.phase === 1) {
                const position = clamp(elapsed / (rate * 0.001 * p.attackMs), 0, 1);
                this.env = this.envStart + (this.envPeak - this.envStart) * position;
                if (position >= 1) {
                    this.phase = p.onMs > 0 || (this.manualCycle && this.manualHold) ? 2 : 3;
                    this.envElapsed = 0;
                }
            } else if (this.phase === 2) {
                const on = p.onMs * (this.aksProfile && this.manualCycle ? AKSCalibration.manual_on_ratio : 1);
                if (elapsed >= rate * 0.001 * on && !(this.manualCycle && this.manualHold)) {
                    this.phase = 3;
                    this.envElapsed = 0;
                }
            } else if (this.phase === 3) {
                this.env = this.envPeak * Math.max(0, 1 - elapsed / this.decayFrames);
                if (this.env === 0) {
                    this.phase = 0;
                    this.envElapsed = 0;
                    this.envRunning = repeat;
                    this.manualCycle = false;
                }
            } else if (!repeat && this.aksProfile) {
                this.envRunning = false;
            } else if (elapsed >= rate * 0.001 * p.offMs) {
                this.envStart = this.envElapsed = 0;
                this.phase = 1;
            }
            return this.env;
        }

        filter(input, g, seed) {
            const stage = this.filterStage;
            const current = 1 - g;
            const feedback = 9.1 * this.p.response;
            const state = current * (stage[2] + g * (stage[1] + g * stage[0]));
            let signal = input * this.p.filterDrive;
            if (feedback > 8) signal += seed * 1e-7;
            let y = clamp((signal - feedback * state) / (1 + feedback * g * g * g), -1, 1);
            for (let pole = 0; pole < 3; pole++) {
                const delta = (y - stage[pole]) * g;
                y = stage[pole] + delta;
                stage[pole] = y + delta;
            }
            return clamp(y, -1, 1) * this.p.filterLevel;
        }

        reverb(input, mixCv) {
            const dispersion = 0.57;
            const a = this.springA[this.springIndexA];
            const b = this.springB[this.springIndexB];
            const diffuseA = this.springDiffuseA - dispersion * a;
            const diffuseB = this.springDiffuseB + dispersion * b;
            this.springDiffuseA = a + dispersion * diffuseA;
            this.springDiffuseB = b - dispersion * diffuseB;
            this.springLowA += this.springDamping * (diffuseA - this.springLowA);
            this.springLowB += this.springDamping * (diffuseB - this.springLowB);
            this.springA[this.springIndexA] = clamp(0.45 * input + this.springFeedbackA * this.springLowA, -1, 1);
            this.springB[this.springIndexB] = clamp(0.45 * input - this.springFeedbackB * this.springLowB, -1, 1);
            if (++this.springIndexA === this.springA.length) this.springIndexA = 0;
            if (++this.springIndexB === this.springB.length) this.springIndexB = 0;
            const start = AKSCalibration.reverb_mix_start;
            const mix = this.aksProfile ? Math.max(0, this.p.reverbMix - start) / (1 - start) +
                AKSCalibration.cv_reference / AKSCalibration.reverb_cv_span * mixCv : this.p.reverbMix + 0.5 * mixCv;
            const wet = clamp(mix, 0, 1);
            return ((1 - wet) * input + wet * 0.5 * (diffuseA + diffuseB)) * this.p.reverbLevel;
        }

        output(channel, input, modulation) {
            const amount = channel === 0 ? this.p.out1Tone : this.p.out2Tone;
            this.toneLow[channel] += this.toneCoeff * (input - this.toneLow[channel]);
            const low = this.toneLow[channel];
            const coloured = input - Math.max(0, amount) * low + Math.min(0, amount) * (input - low);
            const level = channel === 0 ? this.p.out1Level : this.p.out2Level;
            const signal = clamp(coloured, -1, 1);
            return this.aksProfile ? signal * clamp(level + modulation, 0, 1) : signal * modulation * level;
        }
    }
    return { Modules, defaults };
})();
