const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { webcrypto } = require('node:crypto');
const folder = path.resolve(process.argv[2]);
const name = 'ziviSynthiA-SMK37-Pro-Open-0.8.2-alpha2';
const html = fs.readFileSync(path.join(folder, name + '-atualizador.html'), 'utf8');
const matches = [...html.matchAll(/<script([^>]*)>([\s\S]*?)<\/script>/g)];
assert.equal(matches.length, 3);
const [, engine, meta, app] = [null, ...matches.map(item => item[2])];
const metadata = JSON.parse(meta);
const expected = fs.readFileSync(path.join(folder, name + '.fwsc'));
assert.deepEqual(Buffer.from(metadata.data, 'base64'), expected);
const context = { TextEncoder, Uint8Array, setTimeout, clearTimeout, crypto: webcrypto, atob };
vm.createContext(context);
vm.runInContext(engine, context);
const web = context.FM1Web;
const nodes = {};
for (const name of ['connect', 'install', 'backup', 'status', 'progress', 'package', 'disconnect']) {
  nodes[name] = { events: {}, addEventListener(key, fn) { this.events[key] = fn; }, removeAttribute(key) { delete this[key]; } };
}
nodes.package.textContent = meta;
const input = { name: 'SMK37 Pro USB MIDI', state: 'connected', async open() {}, async close() {},
  addEventListener(_, listener) { this.listener = listener; },
  removeEventListener(_, listener) { assert.equal(listener, this.listener); delete this.listener; } };
const outgoing = [];
const initialBuild = '0c704e434b9f1b04e466b7d5c26ea97194507fdbba96cffb633f40d17bba8995';
const previousBuild = 'f4b866bbc68d9df56f62a9d64348959d74bf7ab6f03d87dbe68b800fd428695a';
const reportBuild = '41338dff36dc884c25f895132e08d76883ec67de61268efc9d89116a00fef4a1';
let pkg, phase = 'origin', selected = 'SMK-37 Pro_971', verifiedRead = 0, writeEntered = 0;
let sourceBuild = Buffer.alloc(32, 5), confirmWrite = false;
const request = (address, size) => web.frame(48, [0, address & 255, address >>> 8 & 255, address >>> 16 & 255,
  address >>> 24, size & 255, size >>> 8, 0]);
const reply = data => queueMicrotask(() => input.listener?.({data}));
const output = { name: input.name, state: 'connected', async open(){}, async close(){}, send(bytes) {
  const p = web.parse(bytes);
  outgoing.push(p ? p.command : 'start');
  if (!p) {
    ++writeEntered;
    return reply(request(0, 1));
  }
  if (p.command === 17) {
    const identity = phase === 'origin' ? selected : phase === 'loader' ? pkg.loaderIdentity : pkg.product;
    return reply(web.frame(17, [...Buffer.from(identity), ...new Uint8Array(27 - identity.length)]));
  }
  if (p.command === 49) {
    const id = phase === 'installed' ? Buffer.from(pkg.build_id, 'hex') : sourceBuild;
    const family = selected.includes('Elite') ? 9 : 8;
    return reply(web.frame(49, [70, 77, 67, 49, 1, family, 0, 1, 0, 2, 0, 0, 16, 0, ...id, 0]));
  }
  if (p.command === 50) {
    assert.equal(phase, 'origin');
    assert.deepEqual(Buffer.from(p.data.slice(6)), Buffer.from(pkg.build_id, 'hex'));
    return reply(web.frame(50, [0]));
  }
  assert.equal(p.command, 48);
  if (p.data.length === 9) {
    ++verifiedRead;
    assert.equal(p.data[8], pkg.image[0]);
    return reply(request(phase === 'origin' ? 0xe0000000 : 0xf0000000, 8));
  }
  assert.equal(Buffer.from(p.data.slice(8)).toString('ascii'), 'success\0');
  phase = phase === 'origin' ? 'loader' : 'installed';
} };
const access = {
  inputs: new Map([[0, input]]), outputs: new Map([[0, output]]),
  addEventListener(name, handler) { this[name] = handler; },
  removeEventListener(name) { delete this[name]; }
};
Object.assign(context, {
  document: {getElementById: id => nodes[id]}, navigator: {requestMIDIAccess: async opts => {
    assert.equal(opts.sysex, true);
    return access;
  }}, window: {isSecureContext:true, confirm:()=>confirmWrite}
});
(async () => {
  pkg = await web.prepare(metadata);
  assert.equal(pkg.product, 'SMK-37 Pro_8900822');
  assert.equal(pkg.loaderIdentity, 'ota-FM-1_015');
  const corrupted = Uint8Array.from(expected);
  corrupted[1200] ^= 1;
  assert.throws(() => web.inspectPackage(corrupted));
  assert.throws(() => web.inspectPackage(Uint8Array.from(expected.slice(0, -1))));
  await assert.rejects(web.prepare({...metadata, model: 'elite'}));
  await assert.rejects(web.prepare({...metadata, loader_sha256: '0'.repeat(64)}));
  await assert.rejects(web.prepare({...metadata, sha256: '0'.repeat(64)}));
  await vm.runInContext(app, context);
  assert.equal(nodes.connect.disabled, false);
  nodes.backup.checked = true;
  for (const wrong of ['SMK-37 Pro_971', 'SMK-37 Elite_8000001', pkg.loaderIdentity, 'SMK-37 Pro_8900800', 'SMK-37 Pro_8900801', 'SMK-37 Pro_8900802']) {
    selected = wrong;
    await nodes.connect.events.click();
    assert.equal(nodes.install.disabled, true);
  }
  assert.equal(writeEntered, 0);
  selected = 'SMK-37 Pro_8000001';
  await nodes.connect.events.click();
  assert.equal(nodes.install.disabled, false);
  await nodes.install.events.click();
  assert.equal(writeEntered, 0);
  await nodes.disconnect.events.click();
  assert.equal(nodes.install.disabled, true);
  await nodes.connect.events.click();
  input.state = 'disconnected';
  access.statechange();
  assert.equal(nodes.install.disabled, true);
  input.state = 'connected';

  selected = 'SMK-37 Pro_8900800';
  sourceBuild = Buffer.from(initialBuild, 'hex');
  await nodes.connect.events.click();
  assert.equal(nodes.install.disabled, false);

  selected = 'SMK-37 Pro_8900801';
  sourceBuild = Buffer.from(previousBuild, 'hex');
  await nodes.connect.events.click();
  assert.equal(nodes.install.disabled, false);
  selected = 'SMK-37 Pro_8900802';
  sourceBuild = Buffer.from(reportBuild, 'hex');
  await nodes.connect.events.click();
  assert.equal(nodes.install.disabled, false);
  confirmWrite = true;
  sourceBuild = Buffer.alloc(32, 7);
  await nodes.install.events.click();
  assert.equal(writeEntered, 0);
  sourceBuild = Buffer.from(reportBuild, 'hex');
  await nodes.connect.events.click();
  await nodes.install.events.click();
  assert.equal(verifiedRead, 2);
  assert.equal(writeEntered, 2);
  assert.equal(phase, 'installed');
  assert.equal(nodes.progress.value, 100);
  assert.match(nodes.status.textContent, /confirmado após o reinício/);
  await nodes.connect.events.click();
  assert.equal(nodes.install.disabled, true);
  assert.match(nodes.status.textContent, /já está instalada/);
  console.log('SDK WebUpdate: FWSC and embedded SHA validated, corruption rejected.');
  console.log('Updater simulation: wrong model, modified origin, loader, cancellation and USB disconnect blocked; known 0.8.2-alpha1 completed both stages.');
})().catch(error => { console.error(error); process.exitCode=1; });
