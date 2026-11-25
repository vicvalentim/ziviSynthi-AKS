const assert = require("node:assert/strict");
const { readFileSync } = require("node:fs");
const { webcrypto } = require("node:crypto");
const { test } = require("node:test");
const vm = require("node:vm");

const html = readFileSync(process.env.ZIVI_EDITOR_HTML || process.argv[2] || "build/web/release.html", "utf8");
const scripts = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(match => match[1]);
const source = scripts[1];
const { api, wiring, embedded } = vm.runInNewContext(
    scripts[0] + source.slice(0, source.lastIndexOf("\n(() => {")) + "\n({api: ZiviFirmware, wiring: FeluccaFirmwareTransport, embedded: EmbeddedFirmware});",
    { crypto: webcrypto, TextEncoder, Uint8Array, setTimeout: (f, ms) => setTimeout(f, ms / 50), clearTimeout,
        Date: class extends Date { static now() { return super.now() * 50; } } }
);
const payload = version => Uint8Array.from(Buffer.from(embedded[version], "base64"));
const upgrade = "240,34,36,53,127,247";
const hello = "240,0,50,69,0,0,0,64,127,247";
const full = "240,0,50,69,8,0,0,64,0,126,1,247";
const [currentVersion, previousVersion] = Object.keys(embedded);
const stock = "SMK-37 Pro_016";
const current = wiring.productOf(payload(currentVersion));
const previous = previousVersion ? wiring.productOf(payload(previousVersion)) : "SMK-37 Pro_99300";
const ota = "ota-SMK-37 Pro_900";
const u32 = n => [n & 255, (n >>> 8) & 255, (n >>> 16) & 255, n >>> 24];
const frame = (cmd, body) => [240, ...wiring.pack7([0, 0x59, cmd, body.length, body.length >> 8, 0,
    ...body, ~body.reduce((sum, n) => sum + n, 0) & 255]), 247];

function simulate(origin, destination, { disconnect = false, fast = false, duringStage = () => {} } = {}) {
    const access = { inputs: new Map(), outputs: new Map() };
    const stats = { upgrades: [], replies: [], probes: [] };
    let index = 0;
    function attach(identity, name = "ziviSynthiA Pro", onSwitch = () => {}, opts = {}) {
        const reference = ++index;
        const port = side => ({ id: side + reference, state: "connected", name, async open() {} });
        const input = port("i"), output = port("o");
        access.inputs.set(input.id, input);
        access.outputs.set(output.id, output);
        const detach = () => {
            input.state = output.state = "disconnected";
            access.inputs.delete(input.id);
            access.outputs.delete(output.id);
        };
        const emit = (cmd, body) => input.onmidimessage?.({ data: frame(cmd, body) });
        const request = (address, length) => emit(0x30, [63, ...u32(address), length, 0, 0]);
        const inLoader = identity.startsWith("ota-");
        const end = inLoader ? 0xf0000000 : 0xe0000000;
        output.send = raw => {
            const wire = raw.join();
            if (wire === hello || wire === full) {
                stats.probes.push(identity);
                const id = wire === hello && !inLoader ? identity.replace(/_(\d{3})\d*$/, "_$1") : identity;
                const body = [...Buffer.from(id), ...Array(27 - id.length).fill(0)];
                emit(0x11, body);
            } else if (wire === upgrade) {
                stats.upgrades.push(identity);
                if (opts.disconnect) detach();
                else request(0, 16);
            } else {
                const decoded = wiring.parseFrame(raw);
                if (decoded?.cmd === 0x31 && fast) emit(0x31, [1, 0, 16]);
                if (decoded?.cmd !== 0x30) return;
                assert.equal(decoded.body[0], 63);
                const addr = Buffer.from(decoded.body).readUInt32LE(1);
                stats.replies.push([identity, addr, ...decoded.body.slice(8)]);
                if (!addr) request(end, 8);
                else {
                    assert.equal(addr, end);
                    assert.equal(Buffer.from(decoded.body.slice(8)).toString(), "success\0");
                    detach();
                    onSwitch();
                }
            }
        };
        return { key: JSON.stringify([input.id, output.id]), input, detach };
    }
    const attachLoader = () => attach(ota, "USB MIDI Loader", () => attach(destination));
    const initial = origin.startsWith("ota-") ? attach(origin, "USB MIDI Loader", () => attach(destination), { disconnect }) :
        attach(origin, "ziviSynthiA Pro", () => { duringStage(attach); attachLoader(); }, { disconnect });
    return { access, initial, stats, attach };
}

test("embedded packages have the exact published identities and hashes", async () => {
    const candidate = await api.validate(payload(currentVersion));
    assert.equal(candidate.id, current);
    if (previousVersion) assert.equal((await api.validate(payload(previousVersion))).id, previous);
    const damaged = payload(currentVersion);
    damaged[3400] ^= 1;
    await assert.rejects(api.validate(damaged), /SHA‑256/);
    await assert.rejects(api.validate(damaged.subarray(0, 960)), /Tamanho/);
    const otherBoard = payload(currentVersion);
    otherBoard[47] = 1 + "X".charCodeAt(0);
    await assert.rejects(api.validate(otherBoard), /não pertence/);
});

for (const [origin, version, action, fast] of [
    [stock, currentVersion, "Instalar Felucca", false],
    [previous, currentVersion, "Atualizar firmware", true],
    ...(previousVersion ? [[current, previousVersion, "Reverter firmware", true]] : []),
]) {
    test(action + " only writes to the selected Pro and confirms the returning firmware", async () => {
        const target = await api.validate(payload(version));
        const { access, initial, stats, attach } = simulate(origin, target.id, { fast });
        attach("SMK-37 Elite_99100", "SMK-37 Elite");
        attach(ota, "Other Pro in loader");
        attach(stock, "SMK-37 Pro Bluetooth");
        assert.equal(api.ports(access).length, 3);
        const session = new api.Session(access, initial.key);
        assert.equal(await session.identify(), origin);
        assert.equal(api.operation(origin, target), action);
        const steps = [];
        assert.equal(await session.perform(origin, target, null, phase => steps.push(phase)), target.id);
        assert.notEqual(session.returnPort, initial.key);
        assert.deepEqual(stats.upgrades, [origin, ota]);
        assert.equal(stats.probes.includes("SMK-37 Elite_99100"), false);
        assert.equal(stats.replies.length, 4);
        assert.deepEqual(stats.replies[2].slice(2), Array.from(target.image.subarray(0, 16)));
        assert.equal(steps.at(-1), "done");
    });
}

test("an interrupted write can only resume with the original package", async () => {
    const target = await api.validate(payload(currentVersion));
    const other = previousVersion ? await api.validate(payload(previousVersion)) : { ...target, sha256: "different" };
    const pending = { origin: previous, sha256: target.sha256 };
    const { access, initial, stats } = simulate(ota, target.id);
    assert.equal(api.operation(ota, target, null), "");
    assert.equal(api.operation(ota, other, pending), "");
    const session = new api.Session(access, initial.key);
    await assert.rejects(session.perform(ota, other, pending, () => {}), /não permitem/);
    assert.deepEqual(stats.upgrades, []);
    assert.equal(await session.perform(ota, target, pending, () => {}), current);
    assert.deepEqual(stats.upgrades, [ota]);
});

test("drops, source changes and Open identities do not report success", async () => {
    const target = await api.validate(payload(currentVersion));
    const { access, initial, stats } = simulate(previous, target.id, { disconnect: true });
    const session = new api.Session(access, initial.key);
    assert.equal(api.operation("SMK-37 Pro_8900802", target), "");
    await assert.rejects(session.perform(stock, target, null, () => {}), /identidade.*mudou/);
    assert.deepEqual(stats.upgrades, []);
    const phases = [];
    await assert.rejects(session.perform(previous, target, null, step => phases.push(step)), { code: "lost" });
    assert(!phases.includes("done"));
});

test("two new Pro loaders are ambiguous and neither receives an upgrade command", async () => {
    const target = await api.validate(payload(currentVersion));
    const { access, initial, stats } = simulate(previous, target.id, {
        duringStage: attach => attach(ota, "Another loader"),
    });
    await assert.rejects(new api.Session(access, initial.key).perform(previous, target, null, () => {}), /Mais de um teclado/);
    assert.deepEqual(stats.upgrades, [previous]);
});

test("completing the transfer without the destination identity is not success", async () => {
    const target = await api.validate(payload(currentVersion));
    const { access, initial, stats } = simulate(previous, previous);
    const phases = [];
    await assert.rejects(new api.Session(access, initial.key).perform(previous, target, null, phase => phases.push(phase)),
        { code: "noreturn" });
    assert.deepEqual(stats.upgrades, [previous, ota]);
    assert.equal(phases.at(-1), "reboot");
});
