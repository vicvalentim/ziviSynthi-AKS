/* O catálogo de endereços é inserido pelo gerador do HTML. */
const ZiviDSP = (() => {
    const { SOURCE: sources, DEST: destinations } = MatrixCatalog;
    const addresses = list => Object.fromEntries(list.map(([name], index) => [name, index]));
    const activeCount = list => list.filter(([, , stage]) => stage >= 0).length;
    const capacity = activeCount(sources) * activeCount(destinations);
    const src = addresses(sources);
    const dst = addresses(destinations);
    const ksSignal = 16;
    const usbSignal = ksSignal + 1;
    const clamp = (value, min, max) => Math.min(max, Math.max(min, value));
    const coordinate = (row, col) => String.fromCharCode(65 + col) + (row + 1);
    const supported = (row, col) => (sources[row]?.[2] ?? -1) >= 0 && (destinations[col]?.[2] ?? -1) >= 0;
    const delayed = (row, col) => supported(row, col) && sources[row][2] >= destinations[col][2];

    function scaleNote(note, root, mode) {
        const mask = ScaleCatalog[mode]?.mask ?? 0xfff;
        for (let distance = 0; distance < 12; distance++) {
            const lower = note - distance, upper = note + distance;
            if (lower >= 0 && (mask & (1 << ((lower + 12 - root) % 12)))) return lower;
            if (upper < 128 && (mask & (1 << ((upper + 12 - root) % 12)))) return upper;
        }
        return note;
    }

    const defaults = { tune: 0, shape: 0.5, level: 0.7, mix: 0, lfo: 3.5, cutoff: 1200 };
    const ranges = { tune: [-24, 24], shape: [0, 1], level: [0, 1], mix: [0, 1], lfo: [0.02, 500], cutoff: [5, 10000] };
    const presets = { ...FactoryCatalog,
        initial: { name: "Inicial", pins: [[2, 3, 1], [11, 0, 1], [11, 2, 1], [7, 8, 1]], controls: defaults },
        filter: {
            name: "Rampa filtrada",
            pins: [[2, 7, 1], [9, 3, 1], [11, 0, 1], [11, 2, 1], [7, 8, 1], [5, 13, 2], [11, 1, 1]],
            controls: { ...defaults, mix: 1, cutoff: 560 }
        },
        spring: {
            name: "Envelope e molas",
            pins: [[2, 3, 1], [11, 0, 2], [11, 6, 1], [13, 2, 1], [7, 8, 1]],
            controls: { ...defaults, mix: 0.28, envMode: 1, attackMs: 10, onMs: 65, decayMs: 330 }
        },
        resonance: {
            name: "Filtro ressonante",
            pins: [[6, 7, 2], [5, 13, 2], [9, 3, 1], [11, 0, 1], [11, 2, 1]],
            controls: { ...defaults, cutoff: 650, lfo: 0.35, response: 0.81, filterDrive: 1.6 }
        },
        repeat: {
            name: "Trapezoide cíclico",
            pins: [[2, 7, 1], [9, 3, 1], [11, 0, 1], [10, 13, 2], [11, 6, 1], [13, 2, 2], [7, 8, 1]],
            controls: { ...defaults, mix: 0.78, cutoff: 390, envMode: 2, attackMs: 80, onMs: 100,
                        decayMs: 320, offMs: 290, response: 0.62, reverbMix: 0.86 }
        },
        ks: {
            name: "KS · Sequência e teclado",
            pins: [[2, 3, 1], [11, 0, 1], [15, 8, 1], [7, 9, 1], [3, 2, 1], [8, 15, 1]],
            controls: defaults, sequenceTrigger: true
        },
        ring: {
            name: "Modulação em anel",
            pins: [[2, 4, 1], [5, 5, 1], [12, 3, 1], [11, 0, 1], [11, 2, 1], [7, 8, 1], [11, 1, 1]],
            controls: { ...defaults, mix: 1, lfo: 110 }
        }
    };

    function validateSequence(sequence) {
        const { lengthMs, events } = sequence ?? {};
        if (!Number.isInteger(lengthMs) || lengthMs < 1 || lengthMs > 30000 ||
            !Array.isArray(events) || !events.length || events.length > 256) return null;
        if (events[0]?.length === 4) {
            let previous = -1;
            for (const event of events) {
                if (!Array.isArray(event) || event.length !== 4) return null;
                const [ms, note, velocity, attack] = event;
                if (!Number.isInteger(ms) || ms <= previous || ms >= lengthMs ||
                    !Number.isInteger(note) || note < 0 || note > 127 ||
                    !Number.isInteger(velocity) || velocity < 0 || velocity > 127 ||
                    typeof attack !== "boolean" || (attack && !velocity)) return null;
                previous = ms;
            }
            if (!events[0][3]) return null;
            return { lengthMs, events: events.map(event => event.slice()) };
        }
        if (events.length % 2) return null;
        let previous = -1;
        for (let i = 0; i < events.length; i += 2) {
            const on = events[i], off = events[i + 1];
            if (![on, off].every(event => Array.isArray(event) && event.length === 3 && [...event].every(Number.isInteger))) return null;
            if (on[0] <= previous || off[0] <= on[0] || off[0] >= lengthMs ||
                on[1] < 0 || on[1] > 127 || off[1] !== on[1] ||
                on[2] < 1 || on[2] > 127 || off[2] !== 0) return null;
            previous = off[0];
        }
        return { lengthMs, events: events.map(event => event.slice()) };
    }

    const ksRanges = {
        realtimeSpread: [25, 200], sequenceSpread: [25, 200], speed: [25, 400],
        transpose: [-24, 24], limitSeconds: [1, 30]
    };
    function validateKs(settings) {
        if (!settings || typeof settings.loop !== "boolean") return null;
        const result = { loop: settings.loop };
        for (const [id, [lo, hi]] of Object.entries(ksRanges)) {
            const value = settings[id];
            if (!Number.isInteger(value) || value < lo || value > hi) return null;
            result[id] = value;
        }
        return result;
    }

    function integerIn(value, low, high) {
        return Number.isInteger(value) && value >= low && value <= high;
    }

    function validateStepGrid(grid) {
        if (!grid || !integerIn(grid.length, 1, 64) || !integerIn(grid.bpm, 30, 300) ||
            !integerIn(grid.timingBpm, 30, 300) || !integerIn(grid.division, 0, 7) ||
            !Array.isArray(grid.steps) || grid.steps.length !== 64) return null;
        for (const step of grid.steps) {
            if (!Array.isArray(step) || step.length !== 6) return null;
            const [note, velocity, legato, span, delay, duration] = step;
            if (!integerIn(note, 0, 127) || !integerIn(velocity, 0, 127) || typeof legato !== "boolean" ||
                !integerIn(span, 0, 44118 * 30) || !integerIn(delay, 0, Math.max(0, span - 1)) ||
                !integerIn(duration, 0, span - delay)) return null;
        }
        return { ...grid, steps: grid.steps.map(step => [...step]) };
    }

    function validatePerformance(perf) {
        const fields = { bpm: [30, 300], gate: [10, 95], order: [0, 3], division: [0, 7],
                         chance: [0, 100], root: [0, 11], scale: [0, ScaleCatalog.length - 1] };
        if (!perf || ["enabled", "hold", "freeRun"].some(key => typeof perf[key] !== "boolean") ||
            Object.entries(fields).some(([key, [lo, hi]]) => !integerIn(perf[key], lo, hi))) return null;
        return { ...perf };
    }

    function validatePatch(value) {
        if (!value || ![1, 2, 3].includes(value.version) || !Array.isArray(value.pins) || value.pins.length > capacity ||
            !value.controls || typeof value.controls !== "object" ||
            (value.sequenceTrigger !== undefined && typeof value.sequenceTrigger !== "boolean")) return null;
        const modern = value.version === 3;
        if (modern && ((value.ksRoutes !== undefined && !Array.isArray(value.ksRoutes)) ||
            (value.ksMatrix !== undefined && typeof value.ksMatrix !== "boolean"))) return null;
        const controls = {};
        for (const [id, range] of Object.entries(ranges)) {
            const number = value.controls[id];
            if (!Number.isFinite(number) || number < range[0] || number > range[1]) return null;
            controls[id] = number;
        }
        for (const { id, lo, hi, initial } of ModuleCatalog) {
            let number = value.controls[id];
            if (number === undefined && value.version === 1) {
                number = id === "sineLevel" ? 1 - controls.mix : id === "rampLevel" ? controls.mix : initial;
            }
            if (!Number.isFinite(number) || number < lo || number > hi ||
                ((id === "envMode" || id === "triggerMode") && !Number.isInteger(number))) return null;
            controls[id] = number;
        }
        const used = new Set();
        const pins = [], ksRoutes = [];
        for (const pin of value.pins) {
            if (!Array.isArray(pin) || pin.length !== 3) return null;
            const [row, col, type] = pin;
            if (!Number.isInteger(row) || !Number.isInteger(col) ||
                !supported(row, col) || (type !== 1 && type !== 2) || used.has(row * 16 + col)) return null;
            used.add(row * 16 + col);
            if (!modern && row === src.STICK_Y) ksRoutes.push([col, type]);
            else pins.push([row, col, type]);
        }
        if (modern) {
            const usedRoutes = new Set();
            for (const route of value.ksRoutes ?? []) {
                if (!Array.isArray(route) || route.length !== 2) return null;
                const [col, type] = route;
                if (!Number.isInteger(col) || !supported(src.STICK_Y, col) ||
                    (type !== 1 && type !== 2) || usedRoutes.has(col)) return null;
                usedRoutes.add(col);
                ksRoutes.push([col, type]);
            }
        }
        const patch = { version: 3, pins, controls, ksRoutes, ksMatrix: modern && value.ksMatrix === true,
                        sequenceTrigger: value.sequenceTrigger ?? false };
        if (modern && value.ksAutoTrigger === true) patch.ksAutoTrigger = true;
        if (value.pinColors !== undefined) {
            if (!modern || !Array.isArray(value.pinColors) || value.pinColors.length > pins.length) return null;
            const fullPins = new Set(pins.filter(pin => pin[2] === 1).map(([row, col]) => row * 16 + col));
            const colors = [];
            for (const color of value.pinColors) {
                if (!Array.isArray(color) || color.length !== 3) return null;
                const [row, col, name] = color;
                if (!integerIn(row, 0, 15) || !integerIn(col, 0, 15) ||
                    name !== "red" || !fullPins.delete(row * 16 + col)) return null;
                colors.push([row, col, name]);
            }
            if (colors.length) patch.pinColors = colors;
        }
        if (value.ks !== undefined) {
            const ks = validateKs(value.ks);
            if (!ks) return null;
            patch.ks = ks;
        }
        if (value.sequence !== undefined) {
            const sequence = validateSequence(value.sequence);
            if (!sequence) return null;
            patch.sequence = sequence;
        }
        if (value.usbRoutes !== undefined) {
            if (!Array.isArray(value.usbRoutes) || value.usbRoutes.length > 32) return null;
            const used = new Set();
            const routes = [];
            for (const route of value.usbRoutes) {
                if (!Array.isArray(route) || route.length !== 3) return null;
                const [channel, col, amount] = route;
                const at = channel * 16 + col;
                if (!integerIn(channel, 0, 1) || !integerIn(col, 0, 15) ||
                    !supported(src.INPUT1, col) || !integerIn(amount, 1, 2) || used.has(at)) return null;
                used.add(at);
                routes.push([channel, col, amount]);
            }
            patch.usbRoutes = routes;
        }
        if (value.midiCC !== undefined) {
            if (!Array.isArray(value.midiCC) || value.midiCC.length !== 24 ||
                value.midiCC.some(cc => !integerIn(cc, 0, 127))) return null;
            patch.midiCC = [...value.midiCC];
        }
        if (value.stepGrid !== undefined) {
            patch.stepGrid = validateStepGrid(value.stepGrid);
            if (!patch.stepGrid) return null;
        }
        if (value.performance !== undefined) {
            patch.performance = validatePerformance(value.performance);
            if (!patch.performance) return null;
        }
        if (typeof value.name === "string" && value.name.trim()) patch.name = value.name.trim().slice(0, 64);
        return patch;
    }

    function polyBlep(phase, step) {
        if (phase < step) {
            const t = phase / step;
            return t + t - t * t - 1;
        }
        if (phase > 1 - step) {
            const t = (phase - 1) / step;
            return t * t + t + t + 1;
        }
        return 0;
    }

    function advance(phase, step) {
        const next = phase + step;
        return next >= 1 ? next - 1 : next;
    }

    function pulse(phase, step, duty) {
        const shifted = phase < duty ? phase - duty + 1 : phase - duty;
        return (phase < duty ? 1 : -1) + polyBlep(phase, step) - polyBlep(shifted, step);
    }

    function triangle(phase, duty) {
        return phase < duty ? 2 * phase / duty - 1 : 1 - 2 * (phase - duty) / (1 - duty);
    }

    class Synth {
        constructor(rate) {
            if (!Number.isFinite(rate) || rate < 8000 || rate > 192000) throw new RangeError("Taxa inválida");
            this.rate = rate;
            this.ks = new Ks(rate);
            this.arp = { chance: 100, hold: false, freeRun: false, random: 0xf1854c3d,
                         latched: new Map() };
            this.configureScale(0, 0);
            this.configureArp({ enabled: false, bpm: 120, gate: 60, order: 0 });
            this.modules = new ModuleDSP.Modules(rate);
            this.liveAttack = this.thresholdActive = false;
            this.thresholdProbe = 0;
            this.phase = this.osc2Phase = this.lfoPhase = this.bend = this.stickX = this.stickY = 0;
            this.noise = 0x79b125c3;
            this.pitchNote = 69;
            this.hz = new Float64Array(4);
            this.velocity = new Uint8Array(128);
            this.held = [];
            this.matrix = new Uint8Array(256);
            this.ksRoutes = new Uint8Array(16);
            this.usbRoutes = new Uint8Array(32);
            this.routes = Array.from({ length: 16 }, () => []);
            this.signals = new Float64Array(usbSignal + 2);
            this.nextSignals = new Float64Array(usbSignal + 2);
            this.applyPatch({ ...presets.initial, version: 1 });
        }

        applyPatch(value, keepSequence = false) {
            const patch = validatePatch(value);
            if (!patch) return false;
            this.matrix.fill(0);
            this.ksRoutes.fill(0);
            this.usbRoutes.fill(0);
            this.ksMatrix = patch.ksMatrix;
            for (const [col, type] of patch.ksRoutes) this.ksRoutes[col] = type;
            for (const [row, col, pin] of patch.pins) this.matrix[row * 16 + col] = pin;
            for (const [channel, col, amount] of patch.usbRoutes ?? []) this.usbRoutes[channel * 16 + col] = amount;
            for (let col = 0; col < 16; col++) this.rebuild(col);
            this.controls(patch.controls);
            if (!keepSequence) this.loadPhrase(patch.sequence);
            const settings = patch.ks ?? (!keepSequence && patch.sequence
                ? { sequenceSpread: 100, speed: 100, transpose: 0, loop: true } : null);
            if (settings) this.ks.controls(settings);
            this.ks.sequenceTrigger = patch.sequenceTrigger;
            return true;
        }

        loadPhrase(sequence) {
            if (sequence) this.ks.load(sequence);
            else {
                this.ks.stop();
                this.ks.events.length = 0;
                this.ks.length = 0;
                this.ks.limit = "";
            }
        }

        rebuild(col) {
            const routes = this.routes[col];
            routes.length = 0;
            for (let row = 0; row < 16; row++) {
                const pin = this.matrix[row * 16 + col];
                if (pin === 0) continue;
                const isDelayed = delayed(row, col);
                routes.push([row, isDelayed, (pin === 1 ? 1 : 0.5) * (isDelayed ? 0.72 : 1)]);
            }
            const ks = this.ksRoutes[col];
            if (ks && !this.ksMatrix) routes.push([ksSignal, false, ks === 1 ? 1 : 0.5]);
            for (let channel = 0; channel < 2; channel++) {
                const amount = this.usbRoutes[channel * 16 + col];
                if (amount) routes.push([usbSignal + channel, false, amount === 1 ? 1 : 0.5]);
            }
        }

        setUsbRoute(channel, col, amount) {
            if (!integerIn(channel, 0, 1) || !supported(src.INPUT1, col) || !integerIn(amount, 0, 2)) return;
            this.usbRoutes[channel * 16 + col] = amount;
            this.rebuild(col);
        }

        setKsRoute(col, amount) {
            this.ksRoutes[col] = amount;
            this.rebuild(col);
        }

        setKsMatrix(enabled) {
            if (this.ksMatrix === enabled) return;
            this.ksMatrix = enabled;
            for (let col = 0; col < 16; col++) this.rebuild(col);
        }

        pin(row, col, pin) {
            if (!Number.isInteger(row) || !Number.isInteger(col) || !supported(row, col) ||
                !Number.isInteger(pin) || pin < 0 || pin > 2) return false;
            this.matrix[row * 16 + col] = pin;
            this.rebuild(col);
            return true;
        }

        liveNote() {
            return this.ks.randomHeld ? this.ks.randomNote : this.held.at(-1) ?? this.pitchNote;
        }

        liveVelocity() {
            return this.ks.randomHeld ? 100 : this.held.length ? this.velocity[this.held.at(-1)] : 0;
        }

        record() {
            this.ks.record();
            if (!this.arp.enabled || this.ks.randomHeld)
                this.ks.capture(this.liveNote(), this.liveVelocity(), true);
        }

        random(down) {
            if (this.ks.randomHeld === down) return;
            const priorNote = this.liveNote();
            this.ks.random(down);
            if (down) this.liveAttack = true;
            if (!this.held.length) this.pitchNote = priorNote;
            this.ks.capture(this.liveNote(), this.liveVelocity(), down);
        }

        note(note, velocity) {
            if (!Number.isInteger(note) || note < 0 || note >= 128 ||
                !Number.isInteger(velocity) || velocity < 0 || velocity >= 128) return;
            const priorNote = this.liveNote();
            const old = this.held.indexOf(note);
            if (old !== -1) this.held.splice(old, 1);
            this.velocity[note] = velocity;
            if (velocity !== 0) {
                if (this.arp.hold) {
                    if (this.held.length === 0 && old === -1) this.arp.latched.clear();
                    this.arp.latched.set(note, velocity);
                }
                this.held.push(note);
            }
            if (!this.held.length && !this.ks.randomHeld) this.pitchNote = priorNote;
            const attack = velocity !== 0 && !this.ks.randomHeld;
            if (attack) this.liveAttack = true;
            if (!this.arp.enabled || this.ks.randomHeld)
                this.ks.capture(this.liveNote(), this.liveVelocity(), attack);
        }

        configureScale(root, mode) {
            if (!Number.isInteger(root) || root < 0 || root > 11 ||
                !Number.isInteger(mode) || mode < 0 || mode >= ScaleCatalog.length) return;
            Object.assign(this.arp, { root, mode,
                scaleNotes: Array.from({length: 12}, (_, i) => 60 + root + i)
                    .filter(note => scaleNote(note, root, mode) === note) });
        }

        configureArp({ enabled, bpm, gate, order, division = 2, hold = false, freeRun = false, chance = 100 }) {
            if (typeof enabled !== "boolean" || ![bpm, gate, order, division, chance].every(Number.isInteger) ||
                bpm < 30 || bpm > 300 || gate < 10 || gate > 95 || order < 0 || order > 3 ||
                division < 0 || division > 7 || chance < 0 || chance > 100) return;
            const arp = this.arp;
            if (hold !== arp.hold) {
                arp.latched.clear();
                if (hold) for (const note of this.held) arp.latched.set(note, this.velocity[note]);
            }
            Object.assign(arp, { hold, freeRun, chance });
            if (arp.enabled === enabled && arp.bpm === bpm && arp.gate === gate &&
                arp.order === order && arp.division === division) return;
            const wasEnabled = arp.enabled;
            const triplet = Boolean(division & 1);
            const divisor = bpm * (1 << (division >> 1)) * (triplet ? 3 : 1);
            const period = Math.round(this.rate * (triplet ? 120 : 60) / divisor);
            Object.assign(arp, { enabled, bpm, gate, order, division, period,
                gateFrames: Math.floor(period * gate / 100), phase: 0, index: 0,
                note: 128, sourceNote: 128, velocity: 0 });
            if (this.velocity && (wasEnabled || enabled)) {
                this.liveAttack = !enabled && this.liveVelocity() !== 0;
                if (this.ks.mode === "record")
                    this.ks.capture(this.liveNote(), enabled ? 0 : this.liveVelocity(), this.liveAttack);
            }
        }

        arpRandom(range) {
            const arp = this.arp;
            let state = arp.random;
            state ^= state << 13;
            state ^= state >>> 17;
            state ^= state << 5;
            arp.random = state >>> 0;
            return arp.random % range;
        }

        arpSample() {
            const arp = this.arp;
            const latched = arp.hold && arp.latched.size > 0;
            const generated = !latched && !this.held.length && arp.freeRun;
            const count = latched ? arp.latched.size : generated ? arp.scaleNotes.length : this.held.length;
            if (!count) {
                arp.phase = arp.index = arp.velocity = 0;
                arp.note = arp.sourceNote = 128;
                return false;
            }
            const attack = arp.phase === 0;
            if (attack) {
                const notes = latched ? Array.from(arp.latched.keys()) : generated ? arp.scaleNotes : this.held;
                let source;
                if (arp.order === 3) source = notes[this.arpRandom(count)];
                else if (arp.order === 2) {
                    if (arp.index >= count) arp.index = 0;
                    source = notes[arp.index++];
                } else {
                    const ascending = arp.order === 0;
                    let wrap = notes[0], next = 128;
                    for (const note of notes) {
                        if (ascending ? note < wrap : note > wrap) wrap = note;
                        const follows = ascending ? note > arp.sourceNote : note < arp.sourceNote;
                        if (arp.sourceNote < 128 && follows && (next === 128 || (ascending ? note < next : note > next))) next = note;
                    }
                    source = next === 128 ? wrap : next;
                }
                arp.sourceNote = source;
                arp.note = generated ? source : scaleNote(source, arp.root, arp.mode);
                const audible = arp.chance === 100 || (arp.chance > 0 && this.arpRandom(100) < arp.chance);
                arp.velocity = audible ? (generated ? 100 : latched ? arp.latched.get(source) : this.velocity[source]) : 0;
            } else if (arp.phase === arp.gateFrames) arp.velocity = 0;
            if (++arp.phase === arp.period) arp.phase = 0;
            return attack && arp.velocity !== 0;
        }

        controls(values) {
            const mix = this.mix;
            for (const id of Object.keys(ranges)) {
                if (Number.isFinite(values[id])) this[id] = clamp(values[id], ...ranges[id]);
            }
            if (mix !== this.mix) this.modules.setControls({ sineLevel: 1 - this.mix, rampLevel: this.mix });
            this.modules.setControls(values);
        }

        panic() {
            const priorNote = this.liveNote();
            this.held.length = 0;
            this.velocity.fill(0);
            this.arp.latched.clear();
            this.arp.freeRun = false;
            this.arp.velocity = this.arp.phase = 0;
            this.ks.randomHeld = false;
            this.liveAttack = false;
            this.pitchNote = priorNote;
            this.ks.capture(priorNote, 0, false);
            this.bend = 0;
        }

        bus(values, col) {
            let sum = 0;
            for (const route of this.routes[col]) {
                sum += (route[1] ? this.signals : values)[route[0]] * route[2];
            }
            const limit = col === dst.OSC1_PITCH || col === dst.OSC2_PITCH || col === dst.OSC3_PITCH ? 1.5 : 1;
            return clamp(sum, -limit, limit);
        }

        render(left, right, input1, input2, usb = false) {
            const ks = this.ks;
            const m = this.modules;
            const p = m.p;
            const aks = m.aksProfile;
            const calibration = AKSCalibration;
            const ceiling = this.rate * (aks ? 0.25 : 0.22);
            const lowSensitivity = aks ? calibration.cv_reference / calibration.osc_low_octave : 4;
            const filterSensitivity = aks ? calibration.cv_reference / calibration.filter_octave : 4;
            const stickScale = aks ? calibration.stick_volts / calibration.cv_reference : 1;
            const note = this.liveNote();
            this.pitchNote = note;
            const arp = this.arp;
            const arpeggiated = arp.enabled && !ks.randomHeld;
            const asPitch = value => clamp((value - 69) * (ks.realtimeSpread / 100) / 48 + this.bend / 48, -1.5, 1.5);
            let arpNote = arpeggiated && arp.note < 128 ? arp.note : note, arpVelocity = arp.velocity;
            let velocity = this.liveVelocity() / 127, pitch = asPitch(arpNote);
            const seqSpread = ks.sequenceSpread / 100;
            const hasKsRouting = this.ksMatrix
                ? this.matrix.subarray(src.STICK_Y * 16, (src.STICK_Y + 1) * 16).some(Boolean)
                : this.ksRoutes.some(Boolean);
            const asymmetry = 0.15 + 0.70 * this.shape;
            const duty2 = 0.05 + 0.90 * p.osc2Shape;
            const duty3 = 0.05 + 0.90 * p.osc3Shape;
            const values = this.nextSignals;
            if (!usb) values[usbSignal] = values[usbSignal + 1] = 0;
            const tau = 2 * Math.PI;
            let { phase, osc2Phase, lfoPhase, noise } = this;
            let step = 0, osc2Step = 0, lfoStep = 0, coef = 0;
            let priorPitch = 2, priorOsc2 = 2, priorLfo = 2, priorCutoff = 2, effectiveCutoff = 0;
            let peak = 0, meter = 0, meterSum = 0;
            const panA = (p.out1Pan + 1) * 0.5;
            const panC = (p.out2Pan + 1) * 0.5;
            for (let i = 0; i < left.length; i++) {
                ks.prepareSample();
                const recording = ks.mode === "record";
                let liveTrigger = this.liveAttack;
                if (arpeggiated) {
                    liveTrigger = this.arpSample();
                    if (arp.note < 128 && arp.note !== arpNote) {
                        arpNote = arp.note;
                        pitch = asPitch(arpNote);
                    }
                    velocity = arp.velocity / 127;
                    if (recording && (liveTrigger || arp.velocity !== arpVelocity))
                        ks.capture(arpNote, arp.velocity, liveTrigger);
                    arpVelocity = arp.velocity;
                }
                const seqVelocity = recording ? velocity : ks.velocity / 127;
                const seqNote = recording ? arpNote : ks.note;
                const playback = ks.mode === "play";
                const useSequence = ks.sequenceTrigger || playback && ks.audition;
                let target = useSequence ? seqVelocity : velocity;
                let trigger = useSequence && !recording ? ks.attack : liveTrigger;
                this.liveAttack = false;
                const high = Math.abs(this.thresholdProbe) >= (this.thresholdActive ? 0.10 : 0.18);
                if (p.triggerMode === 1) {
                    target = high ? 1 : 0;
                    trigger = high && !this.thresholdActive;
                }
                this.thresholdActive = high;
                noise ^= noise << 13;
                noise ^= noise >>> 17;
                noise ^= noise << 5;
                const raw = (noise >>> 8) / 8388608 - 1;
                m.noiseLow += m.noiseCoeff * (raw - m.noiseLow);
                const color = p.noiseColor;
                values[src.NOISE] = (raw - Math.max(0, color) * m.noiseLow +
                    Math.min(0, color) * (raw - m.noiseLow)) * p.noiseLevel;
                const sequencePitch = clamp((seqNote - 69 + ks.transpose) * seqSpread / 48, -1.5, 1.5);
                const useKeyboardRouting = playback && useSequence && !hasKsRouting;
                const external1 = input1 && Number.isFinite(input1[i]) ? clamp(input1[i], -1, 1) : 0;
                const external2 = input2 && Number.isFinite(input2[i]) ? clamp(input2[i], -1, 1) : 0;
                const in1 = input1 && !usb ? external1 : useKeyboardRouting ? sequencePitch : pitch;
                const in2 = input2 && !usb ? external2 : useKeyboardRouting ? seqVelocity : velocity;
                if (usb) {
                    values[usbSignal] = external1;
                    values[usbSignal + 1] = external2;
                }
                values[src.INPUT1] = clamp(in1 * p.in1Level, -1.5, 1.5);
                values[src.INPUT2] = clamp(in2 * p.in2Level, -1.5, 1.5);
                values[ksSignal] = sequencePitch;
                values[src.STICK_X] = this.stickX * stickScale;
                values[src.STICK_Y] = this.ksMatrix ? values[ksSignal] : this.stickY * stickScale;
                const env = m.envelope(target, trigger, this.bus(values, dst.ENV_DECAY));
                values[src.TRAPEZOID] = env * p.trapLevel;

                const pitchMod = this.bus(values, dst.OSC1_PITCH);
                if (pitchMod !== priorPitch) {
                    const hz = p.osc1Hz * 2 ** ((this.tune + 48 * pitchMod) / 12);
                    step = clamp(hz, 0.1, ceiling) / this.rate;
                    priorPitch = pitchMod;
                }
                const osc2Mod = this.bus(values, dst.OSC2_PITCH);
                if (osc2Mod !== priorOsc2) {
                    osc2Step = clamp(p.osc2Hz * 2 ** (4 * osc2Mod), 0.1, ceiling) / this.rate;
                    priorOsc2 = osc2Mod;
                }
                const lfoMod = this.bus(values, dst.OSC3_PITCH);
                if (lfoMod !== priorLfo) {
                    lfoStep = clamp(this.lfo * 2 ** (lowSensitivity * lfoMod), 0.02, this.rate * 0.18) / this.rate;
                    priorLfo = lfoMod;
                }
                const warped = phase < asymmetry ? 0.5 * phase / asymmetry
                    : 0.5 + 0.5 * (phase - asymmetry) / (1 - asymmetry);
                values[src.OSC1] = clamp(p.sineLevel * Math.sin(tau * warped) +
                    p.rampLevel * (2 * phase - 1 - polyBlep(phase, step)), -1, 1);
                values[src.OSC2] = clamp(p.osc2Tri * triangle(osc2Phase, duty2) +
                    p.osc2Pulse * pulse(osc2Phase, osc2Step, duty2), -1, 1);
                values[src.OSC3_RECT] = p.osc3Pulse * pulse(lfoPhase, lfoStep, duty3);
                values[src.OSC3_TRI] = p.osc3Tri * triangle(lfoPhase, duty3);
                phase = advance(phase, step);
                osc2Phase = advance(osc2Phase, osc2Step);
                lfoPhase = advance(lfoPhase, lfoStep);

                const cutoffMod = this.bus(values, dst.FILTER_CUTOFF);
                if (cutoffMod !== priorCutoff) {
                    effectiveCutoff = clamp(this.cutoff * 2 ** (filterSensitivity * cutoffMod), 5, ceiling);
                    const g = Math.tan(0.5 * tau * effectiveCutoff / this.rate);
                    coef = g / (1 + g);
                    priorCutoff = cutoffMod;
                }
                values[src.FILTER] = m.filter(this.bus(values, dst.FILTER_IN), coef, raw);
                values[src.ENVELOPE] = this.bus(values, dst.ENV_IN) * env * p.envLevel;
                values[src.RING] = this.bus(values, dst.RING_A) * this.bus(values, dst.RING_B) * p.ringLevel;
                values[src.REVERB] = m.reverb(this.bus(values, dst.REVERB_IN), this.bus(values, dst.REVERB_MIX));

                const cvA = this.bus(values, dst.OUT1_LEVEL);
                const cvC = this.bus(values, dst.OUT2_LEVEL);
                const gainA = aks ? cvA : this.routes[dst.OUT1_LEVEL].length ? Math.max(0, cvA) : 1;
                const gainC = aks ? cvC : this.routes[dst.OUT2_LEVEL].length ? Math.max(0, cvC) : 1;
                const a = m.output(0, this.bus(values, dst.OUT1_IN), gainA);
                this.thresholdProbe = this.bus(values, dst.OUT2_IN);
                const c = m.output(1, this.thresholdProbe, gainC);
                values[src.OUT1] = a;
                values[src.OUT2] = c;
                const meterSample = this.bus(values, dst.METER);
                meter = Math.max(meter, Math.abs(meterSample));
                meterSum += meterSample;
                this.signals.set(values);
                const stereoA = (1 - panA) * a + (1 - panC) * c;
                const stereoC = panA * a + panC * c;
                left[i] = (right ? stereoA : a + c) * 0.24 * this.level;
                if (right) right[i] = stereoC * 0.24 * this.level;
                peak = Math.max(peak, Math.abs(left[i]), right ? Math.abs(right[i]) : 0);
                ks.advance();
            }
            if (left.length) {
                this.hz[0] = step * this.rate;
                this.hz[1] = osc2Step * this.rate;
                this.hz[2] = lfoStep * this.rate;
                this.hz[3] = effectiveCutoff;
            }
            this.phase = phase;
            this.osc2Phase = osc2Phase;
            this.lfoPhase = lfoPhase;
            if (m.env < 1e-12) m.env = 0;
            this.noise = noise;
            this.meterPeak = meter;
            this.meterSum = meterSum;
            return peak;
        }
    }
    return { Synth, sources, destinations, supported, delayed, coordinate, presets, validatePatch, validateSequence, validateStepGrid, scaleNote };
})();
