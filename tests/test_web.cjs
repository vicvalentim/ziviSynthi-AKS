const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");

const html = fs.readFileSync("build/web/index.html", "utf8");
const scripts = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(match => match[1]);
assert.equal(scripts.length, 2);
new vm.Script(scripts[1]);
const { Synth, sources, destinations, supported, delayed, validatePatch, presets, scaleNote } =
    vm.runInNewContext(scripts[0] + "\nZiviDSP;");
assert.equal(sources.length, 16);
assert.equal(destinations.length, 16);
assert.equal(sources[6][1], "NOISE");
assert.equal(destinations[7][1], "FILTER");

const probe = new Synth(48000);
let candidates = 0;
for (let row = 0; row < 16; row++) {
    for (let col = 0; col < 16; col++) {
        assert.equal(probe.pin(row, col, 1), supported(row, col));
        candidates += Number(supported(row, col));
    }
}
assert.equal(candidates, 256);
assert.equal(probe.pin(16, 0, 0), false);
assert.equal(delayed(9, 7), true);
assert.equal(delayed(2, 7), false);

function compareWav(name, patch) {
    const synth = new Synth(48000);
    assert(synth.applyPatch({ ...patch, version: 1 }));
    const native = fs.readFileSync("build/" + name + ".wav");
    assert.equal(native.toString("ascii", 0, 4), "RIFF");
    assert.equal(native.readUInt32LE(24), 48000);
    assert.equal(native.readUInt32LE(40), 96000);

    let frames = 0;
    let maxPeak = 0;
    let maxError = 0;
    for (const [velocity, length] of [[110, 36000], [0, 12000]]) {
        synth.note(69, velocity);
        for (let offset = 0; offset < length; offset += 128) {
            const block = new Float32Array(Math.min(128, length - offset));
            maxPeak = Math.max(maxPeak, synth.render(block));
            for (const sample of block) {
                assert(Number.isFinite(sample));
                const reference = native.readInt16LE(44 + 2 * frames++);
                maxError = Math.max(maxError, Math.abs(reference - Math.trunc(32767 * sample)));
            }
        }
    }
    assert.equal(frames, 48000);
    assert(maxPeak > 0.02, name);
    assert(maxError <= 1, name + " divergência: " + maxError);
}

compareWav("demo-native", presets.initial);
compareWav("demo-filter", {
    pins: [[2, 7, 1], [9, 3, 1], [11, 0, 1], [11, 2, 1], [7, 8, 1]],
    controls: { ...presets.initial.controls, mix: 1, lfo: 110, cutoff: 560 }
});
compareWav("demo-ring", presets.ring);
compareWav("demo-spring", presets.spring);
compareWav("demo-resonance", presets.resonance);
compareWav("demo-repeat", presets.repeat);
console.log("Web DSP: 256 contatos operacionais e 288.000 amostras comparadas com C (tolerância de 1 LSB).");

const initial = { ...presets.initial, version: 1 };
assert(validatePatch(JSON.parse(JSON.stringify(initial))));
assert(!validatePatch({ ...initial, version: 4 }));
assert(!validatePatch({ ...initial, pins: [[16, 0, 1]] }));
assert(!validatePatch({ ...initial, pins: [[2, 0, 1], [2, 0, 2]] }));
assert(!validatePatch({ ...initial, controls: { ...initial.controls, tune: NaN } }));
assert(probe.applyPatch(initial));
assert(!probe.applyPatch({ ...initial, pins: [[0, 16, 1]] }));
assert.equal(probe.matrix[11 * 16], 1);
assert.equal(probe.matrix.reduce((count, pin) => count + Boolean(pin), 0), 4);

const migrated = validatePatch({ ...initial, controls: { ...initial.controls, mix: 0.4 } });
assert.equal(migrated.version, 3);
assert.equal(migrated.controls.sineLevel, 0.6);
assert.equal(migrated.controls.rampLevel, 0.4);
assert(validatePatch(JSON.parse(JSON.stringify(migrated))));
assert(!validatePatch({ ...migrated, controls: { ...migrated.controls, envMode: 1.5 } }));
assert(!validatePatch({ ...migrated, controls: { ...migrated.controls, response: 5 } }));
const spring = new Synth(48000);
assert(spring.applyPatch({ ...initial, pins: [[7, 6, 1], [13, 0, 1]] }));
const impulse = new Float32Array(1600);
const result = new Float32Array(1600);
impulse[0] = 1;
spring.render(result, null, impulse);
assert(result.slice(0, 1200).every(sample => sample === 0));
assert(Math.abs(result[1200]) > 0.01 && Math.abs(result[1440]) > 0.01);

const panel = JSON.parse(html.match(/^const PanelCatalog = (.+);$/m)[1]);
assert.equal(panel.length, 10);
assert.equal(panel.flatMap(rack => rack.dials).filter(Boolean).length, 41);
assert.equal(panel.find(rack => rack.tab === "ENV").dials[7].id, "triggerMode");
assert.deepEqual(panel[0].dials.slice(0, 4).map(dial => dial.color), ["blue", "green", "white", "white"]);
const embedded = html.match(/^const ZIVI_WORKLET = (.+);$/m);
assert(embedded);
let Processor;
const peaks = [];
vm.runInNewContext(JSON.parse(embedded[1]), {
    sampleRate: 48000,
    AudioWorkletProcessor: class {
        constructor() {
            this.port = { postMessage: value => peaks.push(structuredClone(value)) };
        }
    },
    registerProcessor: (name, type) => {
        assert.equal(name, "zivisynthi");
        Processor = type;
    }
});
const worklet = new Processor();
const message = (type, ...values) => worklet.port.onmessage({ data: { type, values } });
const left = new Float32Array(128), right = new Float32Array(128);
function renderWorklet() {
    for (let i = 0; i < 40; i++) assert.equal(worklet.process([], [[left, right]]), true);
}
message("note", 69, 127);
renderWorklet();
assert(peaks.at(-1).output > 0.15);
assert(peaks.at(-1).matrix === 0);
assert.deepEqual(left, right);
for (const [index, frequency] of [440, 440, 3.5, 1200].entries()) {
    assert(Math.abs(peaks.at(-1).hz[index] - frequency) < 1e-9);
}
message("note", 81, 127);
renderWorklet();
assert(Math.abs(peaks.at(-1).hz[0] - 880) < 1e-9);
message("note", 81, 0);
message("pin", 11, 0, 0);
message("pin", 11, 2, 0);
message("pin", 11, 1, 1);
renderWorklet();
assert.equal(peaks.at(-1).output, 0);
assert(peaks.at(-1).matrix > 0.9);
assert(Math.abs(peaks.at(-1).matrixCv) < 0.2);
message("pin", 11, 2, 1);
renderWorklet();
assert(left.every(sample => sample === 0));
assert(right.some(sample => Math.abs(sample) > 0.1));

message("patch", { ...migrated, pins: [[14, 1, 1], [15, 1, 2]] });
message("stick-x", -0.6);
message("stick-y", 0.4);
renderWorklet();
assert(Math.abs(peaks.at(-1).matrixCv + 0.4) < 1e-6);
assert(Math.abs(peaks.at(-1).matrix - 0.4) < 1e-6);
assert.equal(peaks.at(-1).output, 0);
message("stick-x", 0.6);
renderWorklet();
assert(Math.abs(peaks.at(-1).matrixCv - 0.8) < 1e-6);
assert(Math.abs(peaks.at(-1).matrix - 0.8) < 1e-6);
message("pin", 14, 1, 0);
message("pin", 15, 1, 0);
renderWorklet();
assert.equal(peaks.at(-1).matrixCv, 0);
assert.equal(peaks.at(-1).matrix, 0);

message("patch", { ...initial, pins: [[2, 0, 1]] });
renderWorklet();
assert(peaks.at(-1).output > 0.15);
message("panic");
renderWorklet();
assert.equal(peaks.at(-1).output, 0);
assert.equal(peaks.at(-1).matrix, 0);
assert.equal(peaks.at(-1).matrixCv, 0);
message("resume");
renderWorklet();
assert(peaks.at(-1).output > 0.15);
console.log("AudioWorklet: estéreo, pico retificado e leitura bipolar de B, soma de pinos e mute validados.");

const ksSynth = new Synth(8000);
const transport = ksSynth.ks;
ksSynth.record();
const tick = frames => ksSynth.render(new Float32Array(frames));
tick(9000);
assert.equal(transport.mode, "record");
assert.equal(transport.recordFrame, 0);
ksSynth.note(60, 108);
tick(73);
ksSynth.note(60, 0);
tick(21);
ksSynth.note(63, 91);
tick(69);
assert(transport.play());
assert.equal(transport.length, 163);
assert.equal(transport.events.map(event => event.frame).join(","), "0,73,94");
assert.equal(transport.portableSequence().events[2][3], true);
transport.controls({ speed: 200, loop: false });
tick(1);
assert.equal(transport.velocity, 108);
assert.equal(transport.attack, true);
tick(37);
assert.equal(transport.velocity, 0);
tick(10);
assert.equal(transport.note, 63);
tick(35);
assert.equal(transport.mode, "stop");
transport.controls({ loop: true, speed: 100, limitSeconds: 1 });
assert(transport.play());
tick(164);
assert.equal(transport.position, 100);
assert.equal(transport.velocity, 108);

assert.equal(scaleNote(61, 0, 1), 60);
const catalog = vm.runInNewContext(scripts[0] + "\nScaleCatalog;");
assert.equal(catalog.length, 32);
assert.equal(new Set(catalog.map(scale => scale.mask)).size, catalog.length);
assert.equal(scaleNote(65, 0, 7), 64);
assert.equal(scaleNote(66, 0, 7), 66);
assert.equal(scaleNote(127, 0, 31), 126);
for (const [mode, scale] of catalog.entries()) {
    for (let root = 0; root < 12; root++) {
        for (let note = 0; note < 128; note++) {
            const snapped = scaleNote(note, root, mode);
            assert(snapped >= 0 && snapped <= 127);
            assert(scale.mask & (1 << ((snapped + 12 - root) % 12)));
            assert.equal(scaleNote(snapped, root, mode), snapped);
        }
    }
}
const performance = new Synth(8000);
performance.configureArp({ enabled: true, bpm: 120, gate: 50, order: 0 });
performance.note(72, 100);
performance.note(60, 100);
performance.record();
performance.render(new Float32Array(2010));
assert.deepEqual(Array.from(performance.ks.events, e => [e.frame, e.note, e.velocity]),
                 [[0, 60, 100], [1000, 60, 0], [2000, 72, 100]]);
performance.configureArp({ enabled: true, bpm: 120, gate: 50, order: 1 });
performance.render(new Float32Array(1));
assert.equal(performance.arp.note, 72);
performance.configureArp({ enabled: false, bpm: 120, gate: 50, order: 0 });
performance.render(new Float32Array(1));
for (const [division, period] of [4000, 2667, 2000, 1333, 1000, 667, 500, 333].entries()) {
    performance.configureArp({ enabled: false, bpm: 120, gate: 50, order: 0, division });
    assert.equal(performance.arp.period, period);
    assert.equal(performance.liveAttack, false);
}

const probabilistic = new Synth(8000);
probabilistic.configureScale(0, 1);
probabilistic.configureArp({ enabled: true, bpm: 120, gate: 50, order: 0, hold: true });
probabilistic.note(61, 99);
probabilistic.note(61, 0);
probabilistic.render(new Float32Array(1));
assert.equal(probabilistic.arp.note, 60);
assert.equal(probabilistic.arp.velocity, 99);
probabilistic.configureArp({ enabled: true, bpm: 120, gate: 50, order: 3, freeRun: true, chance: 0 });
probabilistic.render(new Float32Array(1));
assert.equal(probabilistic.arp.velocity, 0);
probabilistic.configureArp({ enabled: true, bpm: 120, gate: 50, order: 3, freeRun: true });
probabilistic.render(new Float32Array(probabilistic.arp.period));
assert.equal(probabilistic.arp.velocity, 100);
assert.equal(scaleNote(probabilistic.arp.note, 0, 1), probabilistic.arp.note);
probabilistic.panic();
probabilistic.render(new Float32Array(1));
assert.equal(probabilistic.arp.velocity, 0);

ksSynth.panic();
ksSynth.record();
for (let i = 0; i < 256; i++) {
    ksSynth.note(69, i % 2 ? 0 : 100);
    tick(1);
}
ksSynth.note(70, 111);
assert.equal(transport.mode, "stop");
assert.equal(transport.events.length, 256);
assert.equal(transport.limit, "memory");
ksSynth.record();
tick(8000);
assert.equal(transport.mode, "stop");
assert.equal(transport.limit, "time");
assert.equal(transport.length, 8000);
ksSynth.random(true);
const random = transport.randomNote;
assert(random >= 48 && random <= 77);
ksSynth.random(true);
assert.equal(transport.randomNote, random);
ksSynth.random(false);
assert.equal(transport.randomHeld, false);

message("patch", { ...presets.ks, version: 1 });
message("ks-record");
message("note", 69, 110);
for (let i = 0; i < 187; i++) worklet.process([], [[left, right]]);
message("note", 69, 0);
renderWorklet();
assert.equal(peaks.at(-1).ks.mode, "record");
assert.equal(peaks.at(-1).ks.count, 2);
message("ks-play");
message("note", 57, 110);
function channels(frames) {
    const pair = [new Float32Array(frames), new Float32Array(frames)];
    worklet.process([], [pair]);
    return pair;
}
function crossings(samples) {
    let total = 0;
    for (let i = 1; i < samples.length; i++) {
        if (samples[i] > 0 && samples[i - 1] <= 0) total++;
    }
    return total;
}
let [a, c] = channels(4800);
assert(Math.abs(crossings(a) - 44) <= 1);
assert(Math.abs(crossings(c) - 22) <= 1);
assert.equal(peaks.at(-1).ks.mode, "play");
message("ks-controls", { transpose: 12 });
[a, c] = channels(4800);
assert(Math.abs(crossings(a) - 88) <= 1);
assert(Math.abs(crossings(c) - 22) <= 1);
message("ks-controls", { realtimeSpread: 50, sequenceSpread: 50 });
[a, c] = channels(4800);
assert(Math.abs(crossings(a) - 62) <= 1);
assert(Math.abs(crossings(c) - 31) <= 1);
message("ks-stop");
renderWorklet();
assert.equal(peaks.at(-1).ks.mode, "stop");
assert.equal(peaks.at(-1).ks.count, 2);
console.log("KS: intervalos livres, disparos, velocidade, loop, Random e rotas independentes verificados em Node.");

const plotSource = fs.readFileSync("platform/web/visuals.js", "utf8");
const vernierSource = scripts[1].match(/^const VernierCatalog = .+;$/m)[0];
const { envelopeCurve, filterDb, verniers, responseTaper } = vm.runInNewContext(vernierSource + "\n" + plotSource + "\nZiviVisuals;");
assert(Math.abs(9.1 * responseTaper.value(0.5) - 8) < 1e-9);
for (const value of [0, 0.18, 0.5, 0.875, 0.95, 1])
    assert(Math.abs(responseTaper.value(responseTaper.position(value)) - value) < 1e-9);
for (const [vernier, points] of [[verniers.audio, [[0, 1], [0.6, 261], [0.8, 2088], [1, 10000]]],
                                [verniers.low, [[0, 0.02], [0.8, 63], [1, 500]]],
                                [verniers.filter, [[0, 5], [0.5, 150.855], [1, 10000]]]]) {
    for (const [position, hz] of points) {
        assert(Math.abs(vernier.value(position) - hz) < 1e-8);
        assert(Math.abs(vernier.position(hz) - position) < 1e-8);
    }
    assert(Math.abs(vernier.value(vernier.position(vernier.marks[0])) - vernier.marks[0]) < 1e-8);
}
const { Modules } = vm.runInNewContext(scripts[0] + "\nModuleDSP;");
for (const rate of [44118, 48000]) {
    const modules = new Modules(rate);
    modules.p.response = 1;
    const g = Math.tan(Math.PI * verniers.filter.value(0.5) / rate);
    let previous = 0;
    const crossings = [];
    for (let frame = 0; frame < rate; frame++) {
        const sample = modules.filter(frame === 0 ? 0.01 : 0, g / (1 + g), 0);
        if (frame > rate / 2 && sample > 0 && previous <= 0)
            crossings.push(frame - 1 - previous / (sample - previous));
        previous = sample;
    }
    const hz = rate * (crossings.length - 1) / (crossings.at(-1) - crossings[0]);
    assert(Math.abs(hz - 261) < 0.5, rate + " Hz: " + hz);
}
console.log("Vernier do filtro: posição 5 com auto-oscilação de 261 Hz (±0,5 Hz) a 44.118 e 48.000 Hz.");
const curve = envelopeCurve({ mode: 2, attack: 80, on: 100, decay: 320, off: 290 });
assert.equal(curve.duration, 790);
assert.equal(curve.valueAt(40), 0.5);
assert.equal(curve.valueAt(180), 1);
assert.equal(curve.valueAt(340), 0.5);
assert.equal(curve.valueAt(500), 0);
assert.equal(curve.valueAt(789), 0);
const falloff = filterDb(2400, 48000, 600, 0, 1, 1) - filterDb(1200, 48000, 600, 0, 1, 1);
assert(Math.abs(falloff + 16.10) < 0.03);
console.log("Gráficos: curva do preset cíclico e resposta linear de três polos verificados.");

const factory = vm.runInNewContext(scripts[0] + "\nFactoryCatalog;");
assert.equal(Object.values(factory).filter(preset => preset.bank === "studio").length, 6);
assert.equal(Object.values(factory).filter(preset => preset.bank === "lab").length, 8);
for (const bank of ["performance", "texture", "sequence", "handbook", "handbook_workshop", "keys"])
    assert.equal(Object.values(factory).filter(preset => preset.bank === bank).length, 16);
assert.equal(Object.values(factory).filter(preset => preset.bank === "instrument").length, 11);
assert.equal(Object.values(factory).filter(preset => preset.bank === "exploration").length, 7);
assert.equal(Object.keys(factory).length, 128);
const demoRate = 48000;
let factoryFrames = 0;
for (const [key, preset] of Object.entries(factory)) {
    const patch = JSON.parse(fs.readFileSync("build/presets/" + key + ".json"));
    assert.deepEqual(JSON.parse(JSON.stringify(validatePatch(preset))), patch);
    const synth = new Synth(demoRate);
    synth.modules.setProfile(true);
    assert.equal(patch.controls.out1Pan, -1);
    assert.equal(patch.controls.out2Pan, 1);
    assert(synth.applyPatch(patch));
    if (patch.sequence) {
        assert.equal(synth.ks.mode, "stop");
        assert.equal(synth.ks.events.length, patch.sequence.events.length);
        assert(synth.ks.play());
        assert(!synth.applyPatch({ ...patch, sequence: { ...patch.sequence, events: [[0, 61, 100], [50, 60, 0]] } }));
        assert.equal(synth.ks.mode, "play");
    }
    const wav = fs.readFileSync("build/previews/" + key + ".wav");
    assert.equal(wav.readUInt16LE(22), 2);
    assert.equal(wav.readUInt32LE(24), demoRate);
    const frameCount = wav.readUInt32LE(40) / 4;
    assert.equal(frameCount, demoRate * 6);
    let frame = 0, maxError = 0, maximum = 0, peakRight = 0;
    const render = (frames, fade = false) => {
        while (frames) {
            const length = Math.min(128, frames);
            const left = new Float32Array(length), right = new Float32Array(length);
            synth.render(left, right);
            for (let i = 0; i < length; i++, frame++) {
                const gain = fade && frames - i < 1440 ? Math.fround((frames - i - 1) / 1440) : 1;
                let offset = 44 + 4 * frame;
                for (const sample of [left[i], right[i]]) {
                    const pcm = Math.trunc(Math.fround(Math.fround(sample * gain) * 32767));
                    maxError = Math.max(maxError, Math.abs(wav.readInt16LE(offset) - pcm));
                    maximum = Math.max(maximum, Math.abs(pcm));
                    offset += 2;
                }
                peakRight = Math.max(peakRight, Math.abs(right[i]));
            }
            frames -= length;
        }
    };
    if (patch.sequence) {
        if (patch.pins.some(([row, col]) => row === 8 && col === 15)) {
            synth.note(60, 104);
            render(demoRate * 2);
            synth.note(60, 0);
            synth.note(67, 96);
            render(demoRate * 2);
            synth.note(67, 0);
            render(demoRate);
        } else render(demoRate * 5);
        synth.ks.stop();
        render(demoRate, true);
    } else if (patch.controls.envMode === 2) {
        render(demoRate * 6, true);
    } else if (patch.controls.attackMs >= 100) {
        synth.note(60, 112);
        render(demoRate * 4);
        synth.note(60, 0);
        render(demoRate * 2, true);
    } else {
        [0, 7, 3, 10, 5, 0].forEach((interval, index) => {
            synth.note(60 + interval, 112 - (index % 3) * 10);
            render(demoRate / 3);
            synth.note(60 + interval, 0);
            render(demoRate - demoRate / 3, index === 5);
        });
    }
    assert.equal(frame, frameCount);
    factoryFrames += frame;
    assert(maxError <= 1, key + " divergência PCM: " + maxError);
    assert(maximum > 900 && peakRight > 0.01 && maximum < 32767, `${key}: pico ${maximum}, direita ${peakRight}`);
}
const feedbackPreset = factory["molas-em-retorno"];
const hasReturn = pin => pin[0] === 13 && pin[1] === 6 && pin[2] === 2;
assert(feedbackPreset.pins.some(hasReturn));
const springEnergy = pins => {
    const synth = new Synth(demoRate);
    assert(synth.applyPatch({ ...feedbackPreset, pins }));
    synth.note(60, 110);
    synth.render(new Float32Array(4800), new Float32Array(4800));
    synth.note(60, 0);
    const left = new Float32Array(demoRate * 2);
    const right = new Float32Array(demoRate * 2);
    synth.render(left, right);
    return right.subarray(demoRate / 2).reduce((acc, value) => acc + value * value, 0);
};
const energyWithReturn = springEnergy(feedbackPreset.pins);
const energyWithoutReturn = springEnergy(feedbackPreset.pins.filter(pin => !hasReturn(pin)));
assert(energyWithReturn > energyWithoutReturn * 1.1);
console.log(`Catálogo: ${Object.keys(factory).length} patches portáteis, ${factoryFrames.toLocaleString("pt-BR")} quadros estéreo em paridade C–JS (1 LSB) e retorno de molas verificado.`);
