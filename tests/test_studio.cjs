const assert = require("node:assert/strict");
const fs = require("node:fs");
const test = require("node:test");
const vm = require("node:vm");
const { setTimeout: wait } = require("node:timers/promises");

const html = fs.readFileSync("build/web/index.html", "utf8");
const dsp = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const editor = fs.readFileSync("platform/web/studio.js", "utf8");
const sequenceEditor = fs.readFileSync("platform/web/sequence.js", "utf8");

class Control extends EventTarget {
    value = "";
    dataset = {};
    options = [];
    replaceChildren(...options) { this.options = options; }
    add(option) { this.options.push(option); }
    focus() {}
    trigger(type = "click") { this.dispatchEvent(new Event(type)); }
}
class MidiPort extends EventTarget {
    constructor(id) {
        super();
        this.id = id;
        this.name = "ziviSynthiA Pro";
        this.state = "connected";
    }
    async open() {}
}

function environment(storage = new Map(), timer = setTimeout) {
    const ids = new Map();
    const get = id => {
        if (!ids.has(id)) ids.set(id, new Control());
        return ids.get(id);
    };
    const input = new MidiPort("in"), output = new MidiPort("out");
    const access = Object.assign(new EventTarget(), {
        inputs: new Map([["in", input]]), outputs: new Map([["out", output]]), sysexEnabled: true
    });
    const context = {
        document: { getElementById: get }, window: { isSecureContext: true, confirm: () => true },
        navigator: { requestMIDIAccess: async options => {
            assert.equal(options.sysex, true);
            return access;
        } },
        Option: class { constructor(label, value) { this.label = label; this.value = value; } },
        localStorage: { getItem: key => storage.get(key), setItem: (key, value) => storage.set(key, value) },
        setTimeout: timer, clearTimeout
    };
    const code = [dsp, editor, sequenceEditor, "({ ZiviDSP, ZiviStudioProtocol, ZiviSequences, setupSmkStudio, setupCustomPresets })"].join("\n");
    return { ...vm.runInNewContext(code, context), get, access, input, output };
}
const json = value => JSON.parse(JSON.stringify(value));
const defaultKs = { realtimeSpread: 120, sequenceSpread: 91, speed: 108, transpose: -5, limitSeconds: 30, loop: false };

test("link, read, coalesce edits, write complete patch and disconnect", async () => {
    const { ZiviDSP, ZiviStudioProtocol: proto, get, access, input, output, setupSmkStudio } = environment();
    const { command, patch, unpack, message, u14, u21, crc, read14 } = proto;
    const current = { ...ZiviDSP.presets["passos-ks"], ks: defaultKs };
    const bytes = patch(current, { ...defaultKs, sequenceTrigger: true });
    const roundtrip = unpack([...bytes]);
    assert.equal(roundtrip.controls.cutoff, current.controls.cutoff);
    assert.deepEqual(json(roundtrip.ks), defaultKs);
    assert.deepEqual(json(roundtrip.sequence), json(current.sequence));
    const legato = { lengthMs: 600, events: [[0, 60, 100, true], [150, 67, 90, true], [350, 60, 100, false]] };
    assert.deepEqual(json(unpack([...patch({ ...current, sequence: legato },
                                          { ...defaultKs, sequenceTrigger: true })]).sequence), legato);
    const xy = { ...current, ksMatrix: true, pins: [...current.pins, [15, 8, 2]] };
    const withXY = json(unpack(patch(xy, { ...defaultKs, sequenceTrigger: true })));
    assert.equal(withXY.ksMatrix, true);
    assert.deepEqual(withXY.ksRoutes, json(current.ksRoutes));
    assert.ok(withXY.pins.some(([row, col, amount]) => row === 15 && col === 8 && amount === 2));
    const legacy = json(unpack(patch(current, { ...defaultKs, sequenceTrigger: true }, false)));
    assert.deepEqual(legacy.ksRoutes, json(current.ksRoutes));
    assert.equal(legacy.ksMatrix, false);
    const seen = [];
    let uploaded = [], expectedSize, expectedCrc;
    output.send = raw => {
        const op = raw[5], serial = read14(raw.slice(6)), data = raw.slice(8, -1);
        seen.push({ op, data });
        let answer = [];
        if (op === command.hello) answer = [1, 98, 6, 96];
        if (op === command.read) answer = [...u14(bytes.length), ...u21(crc(bytes)), 0];
        if (op === command.readPart) answer = bytes.slice(read14(data), read14(data) + 96);
        if (op === command.begin) {
            expectedSize = read14(data);
            expectedCrc = data[2] | data[3] << 7 | data[4] << 14;
            uploaded = [];
        }
        if (op === command.part) {
            assert.equal(read14(data), uploaded.length);
            uploaded.push(...data.slice(2));
        }
        if (op === command.commit) {
            assert.equal(uploaded.length, expectedSize);
            assert.equal(crc(uploaded), expectedCrc);
        }
        queueMicrotask(() => input.dispatchEvent(Object.assign(new Event("midimessage"), {
            data: new Uint8Array(message(op | 64, serial, [0, ...answer]))
        })));
    };
    let loaded;
    const edit = setupSmkStudio(() => ({ ...current, ks: { ...defaultKs, sequenceTrigger: true } }),
        value => { loaded = value; return true; });
    get("smk-access").trigger();
    await wait(5);
    get("smk-link").trigger();
    await wait(5);
    assert.equal(get("smk-read").disabled, false);
    assert.equal(get("smk-status").dataset.link, "linked");
    assert.deepEqual(seen.map(item => item.op), [command.hello]);
    get("smk-read").trigger();
    await wait(5);
    assert.deepEqual(json(loaded.sequence), json(current.sequence));
    edit("controls", [{ cutoff: 1100 }]);
    edit("controls", [{ cutoff: 1620 }]);
    edit("pin", [2, 3, 1]);
    edit("pin", [2, 3, 2]);
    edit("ks-route", [8, 2]);
    edit("ks-route", [8, 1]);
    edit("ks-route", [9, 2]);
    edit("ks-matrix", [true]);
    edit("stick-y", [1]);
    edit("arp-settings", [{ enabled: true, bpm: 124, gate: 50, order: 1, division: 5 }]);
    edit("scale-settings", [2, 31]);
    await wait(45);
    const edits = seen.filter(item => item.op === command.controls);
    assert.equal(edits.length, 1);
    assert.deepEqual([...edits[0].data], [...proto.edits({ cutoff: 1620 })]);
    assert.deepEqual([...seen.find(item => item.op === command.pin).data], [2, 3, 2]);
    assert.deepEqual(seen.filter(item => item.op === command.performance).map(item => [...item.data]),
                     [[9, 8, 1], [9, 9, 2], [10, 1], [8, 127, 127], [2, 1, 124, 0, 50, 1, 5], [5, 0, 100], [3, 2, 31]]);
    get("smk-apply").trigger();
    await wait(45);
    assert.deepEqual(uploaded, [...bytes]);
    const priorRead = seen.length;
    edit("controls", [{ cutoff: 1690 }]);
    get("smk-read").trigger();
    assert.equal(get("smk-read").disabled, true);
    await wait(45);
    assert.deepEqual(seen.slice(priorRead, priorRead + 2).map(item => item.op), [command.controls, command.read]);
    assert.equal(get("smk-status").dataset.link, "live");
    const sent = seen.length;
    input.state = "disconnected";
    access.dispatchEvent(new Event("statechange"));
    edit("controls", [{ cutoff: 1300 }]);
    await wait(35);
    assert.equal(seen.length, sent);
    assert.equal(get("smk-apply").disabled, true);
    assert.equal(get("smk-status").dataset.link, "error");
});

test("custom preset persists after reopening, can be edited and deleted", () => {
    const storage = new Map();
    let server = environment(storage);
    const value = { ...server.ZiviDSP.presets["orbita-ks"], ks: defaultKs };
    server.setupCustomPresets(() => value, () => {}, server.ZiviDSP.validatePatch);
    server.get("user-name").value = "Meu timbre";
    server.get("user-save").trigger();
    const id = server.get("user-preset").value;
    assert.ok(id);
    server = environment(storage);
    let loaded;
    server.setupCustomPresets(() => value, patch => { loaded = patch; }, server.ZiviDSP.validatePatch);
    server.get("user-preset").value = id;
    server.get("user-preset").trigger("change");
    assert.equal(loaded.name, "Meu timbre");
    assert.deepEqual(json(loaded.ks), defaultKs);
    assert.deepEqual(json(loaded.sequence), json(value.sequence));
    server.get("user-name").value = "Timbre revisto";
    server.get("user-update").trigger();
    assert.equal(server.get("user-preset").options[1].label, "Timbre revisto");
    server.get("user-delete").trigger();
    assert.equal(server.get("user-preset").options.length, 1);
    assert.equal([...storage.values()][0], "[]");
});

test("3.8 round-trips USB routes, timed steps, performance and MIDI CC", async () => {
    const { ZiviDSP, ZiviStudioProtocol: proto, get, input, output, setupSmkStudio } = environment();
    const { command, patch, unpack, message, u14, u21, crc, read14 } = proto;
    const stepGrid = proto.emptyGrid();
    stepGrid.length = 51;
    stepGrid.steps[50] = [77, 112, true, 5515, 275, 5120];
    const midiCC = proto.defaultCC();
    midiCC[0] = 84;
    const current = { ...ZiviDSP.presets["passos-ks"], ks: { ...defaultKs, sequenceTrigger: true },
        usbRoutes: [[0, 0, 1], [1, 2, 2]], stepGrid, midiCC,
        performance: { enabled: true, bpm: 158, gate: 66, order: 3, division: 5,
                       hold: true, freeRun: false, chance: 28, root: 8, scale: 14 } };
    const bytes = patch(current, current.ks, true, true);
    const restored = unpack(bytes);
    for (const field of ["usbRoutes", "stepGrid", "midiCC", "performance", "sequence"])
        assert.deepEqual(json(restored[field]), json(current[field]));
    assert.equal(unpack(bytes.slice(0, -1)), null);
    const previous = unpack(patch(current, current.ks));
    assert.equal(previous.stepGrid, undefined);
    assert.deepEqual(json(previous.sequence), json(current.sequence));
    const sent = [];
    output.send = raw => {
        const [op, serial, data] = [raw[5], read14(raw.slice(6)), raw.slice(8, -1)];
        sent.push({ op, data });
        let answer = [];
        if (op === command.hello) answer = [1, 98, 7, 96];
        if (op === command.user && data.length === 1 && data[0] === 0) answer = [1, 0, 1];
        if (op === command.read) {
            assert.deepEqual(json(data), [2]);
            answer = [...u14(bytes.length), ...u21(crc(bytes)), 0];
        }
        if (op === command.readPart) answer = bytes.slice(read14(data), read14(data) + 96);
        queueMicrotask(() => input.dispatchEvent(Object.assign(new Event("midimessage"), {
            data: new Uint8Array(message(op | 64, serial, [0, ...answer]))
        })));
    };
    let loaded;
    const edit = setupSmkStudio(() => current, value => { loaded = value; return true; });
    const matrix = get("smk-matrix-preset");
    matrix.value = "active";
    matrix.options = ["active", "0", "1", "2"].map(value => ({value}));
    assert.equal(matrix.disabled, true);
    get("smk-access").trigger();
    await wait(5);
    get("smk-link").trigger();
    await wait(5);
    assert.equal(get("smk-user-save").disabled, false);
    assert.equal(matrix.disabled, false);
    assert.equal(matrix.options[2].disabled, true);
    matrix.value = "1";
    get("smk-matrix-preset-form").trigger("submit");
    assert.equal(sent.some(({ op, data }) => op === command.user && data.join() === "1,1"), false);
    matrix.value = "active";
    get("smk-matrix-preset-form").trigger("submit");
    await wait(15);
    assert.deepEqual(json(loaded.stepGrid), json(stepGrid));
    assert.equal(get("smk-matrix-feedback").textContent, "Patch recebido do SMK.");
    matrix.value = "2";
    get("smk-matrix-preset-form").trigger("submit");
    await wait(15);
    assert.equal(get("smk-matrix-feedback").textContent, "Patch recebido do SMK.");
    get("smk-user-slot").value = "2";
    get("smk-user-read").trigger();
    await wait(15);
    edit("step-cell", [50, 79, 90]);
    edit("usb-route", [0, 6, 2]);
    edit("midi-cc", [8, 107]);
    await wait(60);
    get("smk-user-save").trigger();
    await wait(5);
    assert(sent.find(({ op, data }) => op === command.user && data.join() === "1,2"));
    assert(sent.find(({ op, data }) => op === command.step && data.join() === "1,50,79,90"));
    assert(sent.find(({ op, data }) => op === command.usbRoute && data.join() === "0,6,2"));
    assert(sent.find(({ op, data }) => op === command.midiCC && data.join() === "8,107"));
    assert(sent.find(({ op, data }) => op === command.user && data.join() === "2,2"));
    get("smk-link").trigger();
});

test("3.9 reads STEP transport, playhead and external clock without changing patch transfer", async () => {
    const { ZiviDSP, ZiviStudioProtocol: proto, get, input, output, setupSmkStudio } = environment();
    const { command, message, u14, read14 } = proto;
    const value = { ...ZiviDSP.presets.initial, ks: defaultKs };
    let deliver;
    const status = new Promise(resolve => { deliver = resolve; });
    output.send = raw => {
        const op = raw[5], serial = read14(raw.slice(6));
        let answer = [];
        if (op === command.hello) answer = [1, 98, 8, 96];
        if (op === command.performance && raw[8] === 7) {
            answer = [...Array(18).fill(0), 12, 100, 6, 3, ...u14(126)];
        }
        queueMicrotask(() => input.dispatchEvent(Object.assign(new Event("midimessage"), {
            data: new Uint8Array(message(op | 64, serial, [0, ...answer]))
        })));
    };
    setupSmkStudio(() => value, () => true, deliver);
    get("smk-access").trigger();
    await wait(5);
    get("smk-link").trigger();
    await wait(5);
    assert.equal(get("smk-connection").textContent, "USB vinculado");
    get("smk-apply").trigger();
    try {
        const state = await Promise.race([status, wait(850)]);
        assert.deepEqual(json(state?.step ?? null), {
            recording: true, playing: true, index: 6, clock: { external: true, locked: true, bpm: 126 }
        });
        assert.equal(get("smk-connection").textContent, "Edição ao vivo");
    } finally {
        get("smk-link").trigger();
    }
});

test("3.9.1 keeps the link on a missed poll and reconnects the confirmed Felucca ports", async () => {
    const timer = (fn, ms) => setTimeout(fn, ms === 180 ? 14 : ms === 1200 ? 30 : ms >= 500 ? 25 : ms);
    const { get, access, input, output, setupSmkStudio, ZiviStudioProtocol: p, ZiviDSP } = environment(new Map(), timer);
    let hello = 0, polls = 0, missed = false, rejectEdit = false;
    input.name = output.name = "Felucca";
    access.inputs.set("bluetooth", Object.assign(new MidiPort("bluetooth"), { name: "FM-1 057A Bluetooth" }));
    function attach(rx, tx) {
        tx.send = raw => {
            const code = raw[5];
            let answer = [], status = 0;
            if (code === p.command.hello) {
                hello++;
                answer = [1, 98, 9, 96];
            } else if (code === p.command.performance) {
                if (++polls === 2) {
                    missed = true;
                    return;
                }
                answer = Array(24).fill(0);
            } else if (code === p.command.user) answer = [0, 0, 0];
            else if (code === p.command.controls && rejectEdit) status = 5;
            queueMicrotask(() => rx.dispatchEvent(Object.assign(new Event("midimessage"), {
                data: p.message(code | 64, p.read14(raw.slice(6)), [status, ...answer])
            })));
        };
    }
    attach(input, output);
    const liveEdit = setupSmkStudio(() => ({ ...ZiviDSP.presets.initial, ks: defaultKs }), () => true, () => {});
    try {
        get("smk-access").trigger();
        await wait(8);
        assert.equal(get("smk-input").value, "in");
        get("smk-link").trigger();
        await wait(10);
        get("smk-apply").trigger();
        await wait(155);
        assert(missed);
        assert(polls >= 3);
        assert.equal(hello, 1);
        assert.equal(get("smk-status").dataset.link, "live");
        rejectEdit = true;
        liveEdit("controls", [{ cutoff: 1250 }]);
        await wait(65);
        assert.equal(get("smk-link").textContent, "Desvincular SMK");
        assert.equal(get("smk-status").dataset.link, "error");

        input.state = output.state = "disconnected";
        access.dispatchEvent(new Event("statechange"));
        const replacementIn = Object.assign(new MidiPort("new-in"), { name: "Felucca" });
        const replacementOut = Object.assign(new MidiPort("new-out"), { name: "Felucca" });
        access.inputs.set(replacementIn.id, replacementIn);
        access.outputs.set(replacementOut.id, replacementOut);
        attach(replacementIn, replacementOut);
        access.dispatchEvent(new Event("statechange"));
        await wait(85);
        assert.equal(hello, 2);
        assert.equal(get("smk-input").value, "new-in");
        assert.equal(get("smk-status").dataset.link, "linked");
    } finally {
        get("smk-link").trigger();
    }
});

test("3.9.1 transfers a timed SEQ independently and writes the selected slot using the SEQ command", async () => {
    const { get, input, output, setupSmkStudio, ZiviStudioProtocol: p, ZiviSequences } = environment();
    const grid = p.emptyGrid();
    grid.length = 32;
    grid.steps[17] = [72, 102, true, 4500, 12, 4200];
    const bytes = p.encodeGrid(grid);
    const uploaded = [], commands = [];
    let loaded;
    output.send = raw => {
        const code = raw[5], data = raw.slice(8, -1);
        commands.push({ code, data });
        let answer = [];
        if (code === p.command.hello) answer = [1, 98, 9, 96];
        if (code === p.command.user && data[0] === 0) answer = [1, 0, 0];
        if (code === p.command.step) answer = [...p.u14(bytes.length), ...p.u21(p.crc(bytes))];
        if (code === p.command.readPart) answer = bytes.slice(p.read14(data), p.read14(data) + 96);
        if (code === p.command.part) uploaded.push(...data.slice(2));
        if (code === p.command.commit) {
            assert.deepEqual(uploaded, [...bytes]);
            uploaded.length = 0;
        }
        queueMicrotask(() => input.dispatchEvent(Object.assign(new Event("midimessage"), {
            data: p.message(code | 64, p.read14(raw.slice(6)), [0, ...answer])
        })));
    };
    setupSmkStudio(() => ({ stepGrid: grid }), () => false, undefined, undefined, value => {
        loaded = value;
        return true;
    });
    try {
        get("smk-access").trigger();
        await wait(5);
        get("smk-link").trigger();
        await wait(5);
        get("smk-step-read").trigger();
        await wait(10);
        assert.deepEqual(json(loaded), json(grid));
        get("smk-step-write").trigger();
        await wait(10);
        get("smk-user-slot").value = "0";
        get("smk-step-store").trigger();
        await wait(10);
        assert.deepEqual(commands.filter(({ code }) => code === p.command.begin).map(({ data }) => data.at(-1)), [1, 1]);
        assert(commands.some(({ code, data }) => code === p.command.user && data.join() === "3,0"));
        assert(!commands.some(({ code }) => code === p.command.read));
        const clip = ZiviSequences.makeClip(grid, 17, 18);
        assert.deepEqual(json(ZiviSequences.paste(grid, 5, clip).steps[5]), json(grid.steps[17]));
        assert.deepEqual(json(ZiviSequences.clear(grid, 17, 18).steps[17]), [60, 0, false, 0, 0, 0]);
        assert.equal(grid.steps[17][1], 102);
    } finally {
        get("smk-link").trigger();
    }
});

test("SEQ to KS retains rests, tempo-scaled recorded timing and contiguous legato", () => {
    const { ZiviStudioProtocol: { emptyGrid }, ZiviSequences } = environment();
    const grid = { ...emptyGrid(), length: 5, division: 2, bpm: 120, timingBpm: 60 };
    grid.steps[1] = [60, 80, false, 22059, 8820, 13239];
    grid.steps[2] = [64, 92, true, 22059, 0, 8820];
    grid.steps[4] = [67, 100, false, 0, 0, 0];
    assert.deepEqual(json(ZiviSequences.toKs(grid)), {
        lengthMs: 1250,
        events: [[350, 60, 80, true], [500, 64, 92, false], [600, 64, 0, false],
                 [1000, 67, 100, true], [1188, 67, 0, false]]
    });
    assert.equal(ZiviSequences.toKs(emptyGrid()), null);
    assert.equal(ZiviSequences.toKs({ ...grid, length: 64, bpm: 30, division: 0 }), null);
});

test("red pins survive in a web patch and use the normal pin code on the SMK", () => {
    const { ZiviDSP, ZiviStudioProtocol: protocol } = environment();
    const base = ZiviDSP.validatePatch({ ...ZiviDSP.presets.initial, version: 1 });
    const [row, col] = base.pins[0];
    const colors = [[row, col, "red"]];
    const colored = ZiviDSP.validatePatch({ ...base, pinColors: colors });
    assert.deepEqual(json(colored.pinColors), colors);
    assert.equal(ZiviDSP.validatePatch({ ...base, pinColors: [...colors, ...colors] }), null);
    assert.equal(ZiviDSP.validatePatch({ ...base, pinColors: [[15, 15, "red"]] }), null);
    const settings = { ...defaultKs, sequenceTrigger: false };
    assert.deepEqual(json(protocol.patch(colored, settings)), json(protocol.patch(base, settings)));
});

test("3.9.3 follows physical pin and knob edits and writes panel values", async () => {
    const { ZiviDSP, ZiviStudioProtocol: p, get, input, output, setupSmkStudio } = environment();
    const start = ZiviDSP.validatePatch({ ...ZiviDSP.presets.initial, version: 1 });
    let editor = { ...start, ks: defaultKs, stepGrid: p.emptyGrid() };
    let remote = editor;
    let remoteReads = 0, panelWrites = 0, requestedParts = 0;
    const panel = [4096, 4096, 4096, 0, 0, 1, 0, 0, 0, 120, 0, 0, 0, 0, 8192, 8192];
    let loadedPanel;
    output.send = raw => {
        const op = raw[5], data = raw.slice(8, -1);
        const wire = p.patch(remote, remote.ks, true, true);
        let answer = [];
        if (op === p.command.hello) answer = [1, 98, 10, 96];
        if (op === p.command.user) answer = [0, 0, 0];
        if (op === p.command.performance) answer = Array(24).fill(0);
        if (op === p.command.panel) {
            if (data.length) {
                panel[data[0]] = p.read14(data.slice(1));
                panelWrites++;
            } else answer = panel.flatMap(p.u14);
        }
        if (op === p.command.read) {
            assert.equal(data[0], 3);
            const same = p.read14(data.slice(1)) === wire.length && p.read21(data.slice(3)) === p.crc(wire);
            answer = [...p.u14(same ? 0 : wire.length), ...p.u21(p.crc(wire)), 0];
        }
        if (op === p.command.readPart) {
            requestedParts++;
            const offset = p.read14(data);
            answer = wire.slice(offset, offset + 96);
        }
        queueMicrotask(() => input.dispatchEvent(Object.assign(new Event("midimessage"), {
            data: p.message(op | 64, p.read14(raw.slice(6)), [0, ...answer])
        })));
    };
    const edit = setupSmkStudio(() => editor, () => true, () => {}, null, null, value => {
        editor = value;
        remoteReads++;
        return true;
    }, state => { loadedPanel = state; });
    try {
        get("smk-access").trigger();
        await wait(10);
        get("smk-link").trigger();
        await wait(15);
        get("smk-apply").trigger();
        await wait(240);
        assert.equal(requestedParts, 0);
        assert.equal(loadedPanel[0], 4096);
        remote = { ...remote, controls: { ...remote.controls, osc1Hz: 2088 },
            pins: [...remote.pins, [14, 1, 2]] };
        await wait(480);
        assert.equal(remoteReads, 1);
        assert.equal(editor.controls.osc1Hz, 2088);
        assert(editor.pins.some(pin => pin.join() === "14,1,2"));
        edit("panel-config", [0, 2048]);
        await wait(450);
        assert.equal(panelWrites, 1);
        assert.equal(loadedPanel[0], 2048);
        assert.equal(remoteReads, 1);
    } finally {
        get("smk-link").trigger();
    }
});

test("firmware operations suspend the Studio link and prevent reconnection during writing", async () => {
    const { get, access, input, output, setupSmkStudio, ZiviStudioProtocol: protocol, ZiviDSP } = environment();
    const sent = [];
    output.send = raw => {
        const op = raw[5], serial = protocol.read14(raw.slice(6));
        sent.push(op);
        const data = op === protocol.command.hello ? [1, 98, 6, 96] : [];
        queueMicrotask(() => input.dispatchEvent(Object.assign(new Event("midimessage"), {
            data: new Uint8Array(protocol.message(op | 64, serial, [0, ...data]))
        })));
    };
    const edit = setupSmkStudio(() => ({ ...ZiviDSP.presets.initial, ks: defaultKs }), () => true);
    get("smk-access").trigger();
    await wait(5);
    get("smk-link").trigger();
    await wait(5);
    get("smk-apply").trigger();
    assert.throws(edit.lockFirmware, /Aguarde a transferência/);
    await wait(45);
    const unlock = edit.lockFirmware();
    assert.equal(get("smk-link").disabled, true);
    const before = sent.length;
    get("smk-link").trigger();
    access.dispatchEvent(new Event("statechange"));
    edit("pin", [2, 0, 1]);
    await wait(20);
    assert.equal(sent.length, before);
    unlock();
    assert.equal(get("smk-link").disabled, false);
    get("smk-link").trigger();
    await wait(5);
    assert.equal(sent.at(-1), protocol.command.hello);
    get("smk-link").trigger();
});

test("3.9.4 transfers single edits without reading a full patch and synchronizes joystick ranges", async () => {
    const { get, input, output, setupSmkStudio, ZiviStudioProtocol: p, ZiviDSP } = environment();
    const initial = ZiviDSP.validatePatch({ ...ZiviDSP.presets.initial, version: 1 });
    const editor = { ...initial, ks: defaultKs, stepGrid: p.emptyGrid() };
    const wire = p.patch(editor, defaultKs, true, true);
    assert(p.unpack(wire));
    const sent = [], deltas = [];
    let observed, fullReads = 0, revision = 1;
    const panel = [4096, 4096, 4096, 0, 0, 1, 0, 0, 0, 120, 0, 0, 0, 0, 8192, 8192, 4096, 4096, 8192, 8192];
    output.send = raw => {
        const op = raw[5], data = raw.slice(8, -1);
        sent.push([op, ...data]);
        let answer = [];
        if (op === p.command.hello) answer = [1, 98, 11, 96];
        if (op === p.command.user) answer = [0, 0, 0];
        if (op === p.command.performance && data[0] === 7) answer = Array(24).fill(0);
        if (op === p.command.panel) {
            if (data.length === 1) answer = panel.flatMap(p.u14);
            else panel[data[0]] = p.read14(data.slice(1));
        }
        if (op === p.command.sync) {
            const prior = p.read21(data);
            answer = [0, ...p.u21(revision)];
            if (!prior) answer = [2, ...p.u21(revision), ...p.u14(wire.length), ...p.u21(p.crc(wire)), 0];
            else if (prior !== revision) answer = [1, ...p.u21(revision), ...p.edits({ osc1Hz: 2088 }), 64, 14, 1, 2];
        }
        if (op === p.command.readPart) {
            fullReads++;
            const offset = p.read14(data);
            answer = wire.slice(offset, offset + 96);
        }
        if (op === p.command.profile) answer = [230, 850, 0, 4, 0, 0, 0, 120].flatMap(value =>
            [...p.u21(value), (value >>> 21) & 127, value >>> 28]);
        queueMicrotask(() => input.dispatchEvent(Object.assign(new Event("midimessage"), {
            data: p.message(op | 64, p.read14(raw.slice(6)), [0, ...answer])
        })));
    };
    const edit = setupSmkStudio(() => editor, () => true, () => {}, null, null, () => true,
        values => { observed = values; }, delta => deltas.push(delta));
    const until = async predicate => {
        for (let i = 0; i < 100 && !predicate(); ++i) await wait(16);
        assert(predicate(), JSON.stringify({status: get("smk-status").textContent, recent: sent.slice(-20), deltas, fullReads}));
    };
    try {
        get("smk-access").trigger();
        await wait(5);
        get("smk-link").trigger();
        await until(() => !get("smk-apply").disabled);
        get("smk-apply").trigger();
        await until(() => fullReads === Math.ceil(wire.length / 96));
        const baseline = fullReads;
        revision++;
        await until(() => deltas.length === 1);
        assert.deepEqual(json(deltas[0]), { controls: { osc1Hz: 2088 }, pins: [[14, 1, 2]] });
        edit("panel-config", [16, 2048]);
        edit("stick-x", [0.25, 0.5]);
        await until(() => observed?.[16] === 2048);
        assert(sent.some(command => command.join() === [p.command.performance, 11, 0, ...p.u14(12288)].join()));
        assert.equal(fullReads, baseline);
        assert(!sent.some(([op]) => op === p.command.read));
        await until(() => !get("smk-profile-read").disabled);
        get("smk-profile-read").trigger();
        await until(() => Boolean(get("smk-profile-result").textContent));
        assert.match(get("smk-profile-result").textContent, /DSP máx. 850 µs.*Fila mín. 4/);
    } finally {
        get("smk-link").trigger();
    }
});

test("all 41 catalog controls have valid exact and incremental USB encodings", () => {
    const { ZiviDSP, ZiviStudioProtocol: p } = environment();
    const value = ZiviDSP.validatePatch({ ...ZiviDSP.presets.initial, version: 1 });
    const controls = p.readDelta(p.edits(value.controls)).controls;
    assert.equal(Object.keys(controls).length, 41);
    for (const [id, item] of Object.entries(value.controls)) assert.equal(Math.fround(controls[id]), Math.fround(item), id);
    const pins = p.readDelta([64, 7, 2, 0, 64, 15, 15, 2]).pins;
    assert.deepEqual(json(pins), [[7, 2, 0], [15, 15, 2]]);
    assert.equal(p.readDelta([64, 7, 2]), null);
});

test("3.9.5 writes only changed fields and incorporates sparse state and panel updates", async () => {
    const { get, input, output, setupSmkStudio, ZiviStudioProtocol: p, ZiviDSP } = environment();
    const initial = { ...ZiviDSP.presets["passos-ks"], ks: defaultKs, stepGrid: p.emptyGrid(),
        performance: { enabled: false, hold: false, freeRun: false, bpm: 120, gate: 60,
            order: 0, division: 2, chance: 100, root: 0, scale: 0 }, midiCC: p.defaultCC() };
    const panel = [4096, 4096, 4096, 0, 0, 1, 0, 0, 0, 120, 0, 0, 0, 0, 8192, 8192, 4096, 4096, 8192, 8192, 1];
    const base = 41 * 5 + 270;
    const encode = (value, machine = panel) => {
        const wire = p.patch(value, { ...value.ks, sequenceTrigger: value.sequenceTrigger }, true, true);
        const cut = base + (value.sequence?.events.length ?? 0) * 5;
        return { fixed: [...machine.flatMap(p.u14), ...wire.slice(0, base), ...wire.slice(cut)],
            phrase: wire.slice(base, cut) };
    };
    const before = encode(initial), sent = [], received = [];
    let patchRead, panelRead, revision = 1, payload = [], difference = [];
    output.send = raw => {
        const op = raw[5], data = raw.slice(8, -1);
        sent.push([op, ...data]);
        let answer = [];
        if (op === p.command.hello) answer = [1, 98, 12, 96];
        if (op === p.command.user) answer = [0, 0, 0];
        if (op === p.command.performance && data[0] === 7 && data.length === 1) answer = Array(24).fill(0);
        if (op === p.command.diff && data.length === 3) {
            const known = p.read21(data);
            if (known === revision) answer = [0, ...p.u21(revision)];
            else {
                payload = known ? difference : [...before.fixed, ...before.phrase];
                answer = [known ? 1 : 2, ...p.u21(revision), ...p.u14(payload.length), ...p.u21(p.crc(payload)),
                    ...(payload.length <= 87 ? payload : [])];
            }
        }
        if (op === p.command.diff && data.length === 2) {
            const offset = p.read14(data);
            answer = payload.slice(offset, offset + 96);
        }
        queueMicrotask(() => input.dispatchEvent(Object.assign(new Event("midimessage"), {
            data: p.message(op | 64, p.read14(raw.slice(6)), [0, ...answer])
        })));
    };
    const edit = setupSmkStudio(() => initial, value => { patchRead = value; return true; }, () => {},
        null, null, () => true, values => { panelRead = values; }, delta => received.push(delta));
    const until = async predicate => {
        for (let i = 0; i < 100 && !predicate(); ++i) await wait(16);
        assert(predicate(), JSON.stringify({ status: get("smk-status").textContent, sent: sent.slice(-15) }));
    };
    try {
        get("smk-access").trigger();
        await wait(5);
        get("smk-link").trigger();
        await until(() => !get("smk-read").disabled);
        get("smk-read").trigger();
        await until(() => patchRead && panelRead);
        assert.equal(panelRead[20], 1);
        assert.equal(patchRead.controls.cutoff, initial.controls.cutoff);
        edit("ks-controls", [{ ...defaultKs, sequenceTrigger: initial.sequenceTrigger, speed: 133 }]);
        edit("arp-settings", [{ ...initial.performance, bpm: 147 }]);
        edit("controls", [{ cutoff: initial.controls.cutoff }]);
        await until(() => sent.filter(([op]) => op === p.command.fields).length === 2);
        assert.deepEqual(sent.filter(([op]) => op === p.command.fields),
            [[p.command.fields, 0, 2, ...p.u14(133)], [p.command.fields, 1, 3, ...p.u14(147)]]);
        assert(!sent.some(([op]) => op === p.command.controls));
        const steps = p.emptyGrid();
        steps.steps[3] = [63, 108, true, 1000, 40, 530];
        const after = encode({ ...initial, controls: { ...initial.controls, cutoff: 1800 },
            ks: { ...defaultKs, speed: 133 }, performance: { ...initial.performance, bpm: 147, root: 6, scale: 2 },
            stepGrid: steps, usbRoutes: [[0, 2, 1]], pins: [...initial.pins, [15, 12, 2]],
            midiCC: initial.midiCC.map((cc, index) => index === 2 ? 51 : cc) }, panel.map((v, i) => i === 20 ? 0 : v));
        difference = [127, 127];
        after.fixed.forEach((value, index) => {
            if (value !== before.fixed[index]) difference.push(...p.u14(index), 1, value);
        });
        revision = 2;
        await until(() => received.length === 1);
        const { previous, current } = received[0];
        assert.equal(previous.controls.cutoff, initial.controls.cutoff);
        assert.equal(current.controls.cutoff, 1800);
        assert.equal(current.ks.speed, 133);
        assert.equal(current.performance.root, 6);
        assert.equal(current.midiCC[2], 51);
        assert.deepEqual(json(current.stepGrid.steps[3]), steps.steps[3]);
        assert.deepEqual(json(current.usbRoutes), [[0, 2, 1]]);
        assert(current.pins.some(([row, col, pin]) => row === 15 && col === 12 && pin === 2));
        assert.equal(panelRead[20], 0);
        assert(!sent.some(([op]) => [p.command.read, p.command.begin, p.command.sync].includes(op)));
    } finally {
        get("smk-link").trigger();
    }
});
