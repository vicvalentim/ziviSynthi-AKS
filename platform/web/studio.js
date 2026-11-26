const ZiviStudioProtocol = (() => {
    const signature = [0xf0, 0x7d, 0x5a, 0x53, 1];
    const command = { hello: 1, begin: 2, part: 3, commit: 4, controls: 5, pin: 6, ks: 7, transport: 8, read: 9, readPart: 10, performance: 11, step: 12, user: 13, usbRoute: 14, midiCC: 15, panel: 16, sync: 17, profile: 18, diff: 19, fields: 20 };
    const ids = ["tune", "shape", "level", "mix", "lfo", "cutoff", ...ModuleCatalog.map(param => param.id)];
    const bounds = [[-24, 24], [0, 1], [0, 1], [0, 1], [0.02, 500], [5, 10000],
                    ...ModuleCatalog.map(({ lo, hi }) => [lo, hi])];
    const scratch = new DataView(new ArrayBuffer(4));
    const u14 = value => [value & 127, (value >>> 7) & 127];
    const u21 = value => [...u14(value), (value >>> 14) & 127];
    const read14 = bytes => bytes[0] | (bytes[1] << 7);
    const read21 = bytes => read14(bytes) | (bytes[2] << 14);
    const read32 = (bytes, offset) => (bytes[offset] | (bytes[offset + 1] << 7) | (bytes[offset + 2] << 14) |
        (bytes[offset + 3] << 21) | (bytes[offset + 4] << 28)) >>> 0;
    function crc(bytes) {
        let value = 0xffff;
        for (const byte of bytes) {
            value ^= byte << 8;
            for (let bit = 0; bit < 8; bit++) value = ((value << 1) ^ (value & 0x8000 ? 0x1021 : 0)) & 0xffff;
        }
        return value;
    }
    const f32 = value => {
        scratch.setFloat32(0, value, true);
        const bits = scratch.getUint32(0, true);
        return [0, 7, 14, 21, 28].map(shift => (bits >>> shift) & 127);
    };
    const ks = settings => [
        ...u14(settings.realtimeSpread), ...u14(settings.sequenceSpread), ...u14(settings.speed),
        settings.transpose + 24, settings.limitSeconds, Number(settings.loop), Number(settings.sequenceTrigger)
    ];
    const emptyStep = () => [60, 0, false, 0, 0, 0];
    const emptyGrid = () => ({ length: 16, bpm: 120, timingBpm: 120, division: 4,
                              steps: Array.from({ length: 64 }, emptyStep) });
    const defaultPerformance = { enabled: false, hold: false, freeRun: false, bpm: 120, gate: 60,
                                 order: 0, division: 2, chance: 100, root: 0, scale: 0 };
    const defaultCC = () => [...Array.from({ length: 16 }, (_, i) => 16 + i), 70, 71, 72, 73, 76, 77, 78, 79];
    const gridSize = 6 + 64 * 12;
    const extensionSize = 32 + 9 + gridSize + 24;
    const panelBytes = 42, patchBase = ids.length * 5 + 270;
    const fixedBytes = panelBytes + patchBase + extensionSize;

    function encodeGrid(grid) {
        return [grid.length, ...u14(grid.bpm), ...u14(grid.timingBpm), grid.division,
            ...grid.steps.flatMap(([note, velocity, legato, span, delay, duration]) =>
                [note, velocity, Number(legato), ...u21(span), ...u21(delay), ...u21(duration)])];
    }

    function decodeGrid(bytes) {
        if (bytes.length !== gridSize) return null;
        const grid = { length: bytes[0], bpm: read14(bytes.slice(1)),
            timingBpm: read14(bytes.slice(3)), division: bytes[5], steps: [] };
        for (let p = 6; p < bytes.length; p += 12) {
            if (bytes[p + 2] > 1) return null;
            grid.steps.push([bytes[p], bytes[p + 1], Boolean(bytes[p + 2]), read21(bytes.slice(p + 3)),
                read21(bytes.slice(p + 6)), read21(bytes.slice(p + 9))]);
        }
        return grid;
    }

    function extension(value) {
        const matrix = new Uint8Array(32);
        for (const [channel, col, amount] of value.usbRoutes ?? []) matrix[channel * 16 + col] = amount;
        const perf = value.performance ?? defaultPerformance;
        const grid = value.stepGrid ?? emptyGrid();
        return [...matrix, Number(perf.enabled) | Number(perf.hold) << 1 | Number(perf.freeRun) << 2,
                ...u14(perf.bpm), perf.gate, perf.order, perf.division, perf.chance, perf.root, perf.scale,
                ...encodeGrid(grid), ...(value.midiCC ?? defaultCC())];
    }

    function readExtension(bytes, start) {
        const usbRoutes = bytes.slice(start, start + 32).flatMap((amount, i) =>
            amount ? [[i >> 4, i & 15, amount]] : []);
        let p = start + 32;
        const flags = bytes[p];
        const performance = { enabled: Boolean(flags & 1), hold: Boolean(flags & 2), freeRun: Boolean(flags & 4),
            bpm: read14(bytes.slice(p + 1)), gate: bytes[p + 3], order: bytes[p + 4], division: bytes[p + 5],
            chance: bytes[p + 6], root: bytes[p + 7], scale: bytes[p + 8] };
        p += 9;
        const stepGrid = decodeGrid(bytes.slice(p, p + gridSize));
        return stepGrid && { usbRoutes, performance, stepGrid, midiCC: bytes.slice(p + gridSize) };
    }

    function patch(value, settings, xy = true, extended = false) {
        const matrix = new Uint8Array(256);
        for (const [row, col, pin] of value.pins) matrix[16 * row + col] = pin;
        if (xy || !value.ksMatrix) {
            for (const [col, type] of value.ksRoutes ?? [])
                matrix[240 + col] += xy ? 3 * type : type;
        }
        const events = value.sequence?.events ?? [];
        const states = events[0]?.length === 4;
        return [...ids.flatMap(id => f32(value.controls[id])), ...matrix, ...ks(settings),
                ...u21(value.sequence?.lengthMs ?? 0), Number(states) | (xy ? value.ksMatrix ? 6 : 2 : 0) | (extended ? 8 : 0),
                ...events.flatMap(([ms, note, velocity, attack]) => [...u21(ms | (attack ? 1 << 20 : 0)), note, velocity]),
                ...(extended ? extension(value) : [])];
    }
    function unpack(bytes) {
        const float = p => {
            scratch.setUint32(0, read21(p) | (p[3] << 21) | (p[4] << 28), true);
            return scratch.getFloat32(0, true);
        };
        const matrix = ids.length * 5;
        const settings = matrix + 256;
        const eventsStart = settings + 14;
        const flags = bytes[settings + 13], xy = Boolean(flags & 2);
        const eventsEnd = bytes.length - (flags & 8 ? extensionSize : 0);
        if (eventsEnd < eventsStart || (eventsEnd - eventsStart) % 5 || flags > 15 || (!xy && (flags & 12))) return null;
        const controls = Object.fromEntries(ids.map((id, i) => {
            const value = float(bytes.slice(i * 5, i * 5 + 5));
            const [lo, hi] = bounds[i];
            return [id, value === Math.fround(lo) ? lo : value === Math.fround(hi) ? hi : value];
        }));
        const ksRoutes = [], pins = [];
        for (let i = matrix; i < settings; i++) {
            const address = i - matrix, row = address >> 4, col = address & 15;
            let type = bytes[i];
            if (xy && row === 15) {
                if (type > 8) return null;
                const route = Math.floor(type / 3);
                if (route) ksRoutes.push([col, route]);
                type %= 3;
            }
            if (type) pins.push([row, col, type]);
        }
        const sequenceTrigger = Boolean(bytes[settings + 9]);
        const ks = {
            realtimeSpread: read14(bytes.slice(settings)), sequenceSpread: read14(bytes.slice(settings + 2)),
            speed: read14(bytes.slice(settings + 4)), transpose: bytes[settings + 6] - 24,
            limitSeconds: bytes[settings + 7], loop: Boolean(bytes[settings + 8])
        };
        const patch = { version: xy ? 3 : 2, controls, pins, ks, ksRoutes, ksMatrix: Boolean(flags & 4), sequenceTrigger };
        const format = flags & 1;
        if (eventsEnd > eventsStart) {
            const lengthMs = read21(bytes.slice(settings + 10));
            const events = [];
            for (let i = eventsStart; i < eventsEnd; i += 5) {
                const clock = read21(bytes.slice(i, i + 3));
                const event = [format ? clock & 0xfffff : clock, bytes[i + 3], bytes[i + 4]];
                if (format) event.push(Boolean(clock & (1 << 20)));
                events.push(event);
            }
            patch.sequence = { lengthMs, events };
        }
        if (flags & 8) {
            const data = readExtension(bytes, eventsEnd);
            if (!data) return null;
            Object.assign(patch, data);
        }
        return ZiviDSP.validatePatch(patch);
    }
    function edits(changes) {
        return ids.flatMap((id, index) => Object.hasOwn(changes, id) ? [index, ...f32(changes[id])] : []);
    }
    function readDelta(bytes) {
        const controls = {}, pins = [];
        for (let offset = 0; offset < bytes.length;) {
            const id = bytes[offset++];
            if (id === 64) {
                const [row, col, amount] = bytes.slice(offset, offset + 3);
                if (offset + 3 > bytes.length || row > 15 || col > 15 || amount > 2) return null;
                pins.push([row, col, amount]);
                offset += 3;
                continue;
            }
            if (id >= ids.length || offset + 5 > bytes.length || bytes[offset + 4] > 15) return null;
            scratch.setUint32(0, read32(bytes, offset), true);
            let value = scratch.getFloat32(0, true);
            const [lo, hi] = bounds[id];
            if (!(value >= Math.fround(lo) && value <= Math.fround(hi))) return null;
            if (value === Math.fround(lo)) value = lo;
            if (value === Math.fround(hi)) value = hi;
            controls[ids[id]] = value;
            offset += 5;
        }
        return { controls, pins };
    }
    function applyDiff(kind, bytes, previous) {
        let fixed, phrase;
        if (kind === 2) {
            fixed = bytes.slice(0, fixedBytes);
            phrase = bytes.slice(fixedBytes);
        } else {
            if (kind !== 1 || !previous || bytes.length < 2) return null;
            const length = read14(bytes);
            if (length !== 0x3fff && (length > 1280 || length > bytes.length - 2 || length % 5)) return null;
            const end = length === 0x3fff ? bytes.length : bytes.length - length;
            phrase = length === 0x3fff ? previous.phrase : bytes.slice(end);
            fixed = previous.fixed.slice();
            for (let offset = 2; offset < end;) {
                if (offset + 3 > end) return null;
                const at = read14(bytes.slice(offset, offset + 2)), count = bytes[offset + 2];
                offset += 3;
                if (!count || at + count > fixed.length || offset + count > end) return null;
                for (let i = 0; i < count; ++i) fixed[at + i] = bytes[offset + i];
                offset += count;
            }
        }
        if (fixed.length !== fixedBytes || phrase.length > 1280 || phrase.length % 5) return null;
        const value = unpack([...fixed.slice(panelBytes, panelBytes + patchBase), ...phrase, ...fixed.slice(panelBytes + patchBase)]);
        if (!value) return null;
        const panel = Array.from({ length: panelBytes / 2 }, (_, i) => read14(fixed.slice(i * 2, i * 2 + 2)));
        return { fixed, phrase, value, panel };
    }
    function message(code, serial, data) {
        return [ ...signature, code, ...u14(serial), ...data, 0xf7 ];
    }
    function reply(bytes) {
        if (bytes.length < 10 || bytes.at(-1) !== 0xf7 || !signature.every((byte, i) => byte === bytes[i])) return null;
        if (bytes[5] < 64) return null;
        return { code: bytes[5] - 64, serial: read14(bytes.slice(6)), status: bytes[8], data: [...bytes.slice(9, -1)] };
    }
    return { command, edits, ks, message, patch, unpack, reply, u14, u21, read14, read21, read32, readDelta, applyDiff, crc, emptyGrid, defaultCC, encodeGrid, decodeGrid };
})();

function setupSmkStudio(snapshot, loadPatch, performanceState, onPerformanceOptions, onGrid, onRemotePatch, onPanel, onDelta, loadLocalPreset) {
    const { command, edits, ks, message, patch, unpack, reply, u14, u21, read14, read21, read32, readDelta, applyDiff, crc } = ZiviStudioProtocol;
    const get = id => document.getElementById("smk-" + id);
    const button = get("access"), link = get("link"), apply = get("apply"), read = get("read");
    const rx = get("input"), tx = get("output"), status = get("status");
    const connection = get("connection");
    const userSlot = get("user-slot"), userRead = get("user-read"), userSave = get("user-save");
    const userWrite = get("user-write"), stepRead = get("step-read"), stepWrite = get("step-write");
    const stepStore = get("step-store");
    const matrixForm = get("matrix-preset-form"), matrixPreset = get("matrix-preset");
    const matrixLoad = get("matrix-load"), matrixFeedback = get("matrix-feedback");
    const matrixHardware = get("matrix-hardware");
    let matrixLoading = false;
    const profile = get("profile"), profileRead = get("profile-read"), profileReset = get("profile-reset");
    let supportsExtended = false, supportsStepTelemetry = false, supportsGrid = false, supportsSync = false;
    let syncAfter = 0, revision = 0, supportsDelta = false;
    let lastSync;
    let supportsSparse = false, mirror, lastTelemetry;
    const seenEdits = new Map();
    const portKey = "zivisynthiA.studio.ports.v1";
    let preferred = {};
    try { preferred = JSON.parse(localStorage.getItem(portKey)) || {}; } catch {}
    const savePortSelection = () => {
        const describe = port => ({ id: port.id, name: port.name, manufacturer: port.manufacturer });
        preferred = { input: describe(input), output: describe(output) };
        try { localStorage.setItem(portKey, JSON.stringify(preferred)); } catch {}
    };
    const jobs = new Map();
    let scaleCount = ScaleCatalog.length, advancedArp = true;
    let access, input, output, request, partSize, supportsPerformance, supportsXY, busy = false, live = false, serial = 0, timer = 0, telemetryTimer = 0;
    let patchPending = false, readPending = false, firmwareLock = false;
    let wantLink = false, reconnectTimer = 0, pollFailures = 0, openingFailures = 0, telemetryReadout = false;
    const writePending = () => [...jobs.keys()].some(key => key !== "performance-status");

    function scheduleReconnect() {
        if (!wantLink || input || reconnectTimer || !rx.value || !tx.value) return;
        reconnectTimer = setTimeout(() => {
            reconnectTimer = 0;
            if (!wantLink || input) return;
            if (busy) scheduleReconnect();
            else void openLink();
        }, Math.min(8000, 500 * 2 ** Math.min(openingFailures, 4)));
    }
    const report = (text, mode = "idle") => {
        if (status.textContent !== text) status.textContent = text;
        status.dataset.link = mode;
        if (connection) {
            const label = { idle: "USB desconectado", linked: "USB vinculado", live: "Edição ao vivo",
                busy: "USB transferindo", retry: "USB · aguardando", error: input ? "USB · ver mensagem" : "Verificar USB" };
            connection.textContent = label[mode];
            connection.dataset.link = mode;
        }
    };

    function updateActions() {
        const transfer = patchPending || readPending || busy;
        if (matrixPreset && matrixLoad) {
            if (matrixHardware) matrixHardware.disabled = !input;
            if (!input && loadLocalPreset && matrixPreset.value && !matrixPreset.value.startsWith("factory:")) matrixPreset.value = "";
            matrixPreset.disabled = (!input && !loadLocalPreset) || transfer || matrixLoading;
            matrixLoad.disabled = matrixPreset.disabled || (loadLocalPreset && !matrixPreset.value);
        }
        apply.disabled = read.disabled = !input || transfer;
        link.disabled = firmwareLock || transfer || (!input && !wantLink && (!rx.value || !tx.value));
        rx.disabled = tx.disabled = firmwareLock || !access || Boolean(input) || busy;
        button.disabled = firmwareLock || Boolean(access);
        for (const control of [userSlot, userRead, userSave, userWrite])
            if (control) control.disabled = !input || !supportsExtended || transfer;
        for (const control of [stepRead, stepWrite, stepStore])
            if (control) control.disabled = !input || !supportsGrid || transfer;
        if (profile) profile.hidden = !input || !supportsDelta;
        for (const control of [profileRead, profileReset])
            if (control) control.disabled = !input || !supportsDelta || transfer;
    }

    function disconnect(reason, mode = "idle") {
        if (input) input.removeEventListener("midimessage", receive);
        setMatrixSlots([]);
        if (matrixFeedback) matrixFeedback.textContent = "";
        input = output = undefined;
        request?.finish(new Error(reason));
        clearTimeout(timer);
        clearTimeout(telemetryTimer);
        timer = telemetryTimer = 0;
        jobs.clear();
        live = patchPending = readPending = false;
        performanceState?.(null);
        scaleCount = ScaleCatalog.length;
        supportsExtended = supportsStepTelemetry = supportsGrid = telemetryReadout = false;
        supportsSync = supportsDelta = false;
        supportsSparse = false;
        revision = 0;
        mirror = lastTelemetry = undefined;
        seenEdits.clear();
        syncAfter = 0;
        lastSync = undefined;
        onPanel?.(null, false);
        advancedArp = true;
        onPerformanceOptions?.(scaleCount, true, advancedArp);
        apply.textContent = "Aplicar patch e editar ao vivo";
        link.textContent = wantLink ? "Cancelar reconexão" : "Vincular SMK";
        updateActions();
        report(reason, mode);
        scheduleReconnect();
    }

    function fill(select, ports, prior) {
        const before = select.value;
        const available = [...ports.values()].filter(port => port.state === "connected");
        select.replaceChildren(new Option("Selecionar porta", ""));
        for (const port of available) select.add(new Option(port.name || port.id, port.id));
        const candidates = prior ? available.filter(port => port.name === prior.name && port.manufacturer === prior.manufacturer)
            : available.filter(port => /zivisynthi|smk[ -]?37|^felucca(?: usb)?$/i.test(port.name ?? "") && !/bluetooth/i.test(port.name ?? ""));
        select.value = available.find(port => port.id === before)?.id ??
            available.find(port => prior?.id === port.id)?.id ?? (candidates.length === 1 ? candidates[0].id : "");
    }

    function portsChanged() {
        fill(rx, access.inputs, preferred.input);
        fill(tx, access.outputs, preferred.output);
        if (input?.state === "disconnected" || output?.state === "disconnected") {
            disconnect("USB interrompido. Aguardando o retorno do SMK.", "error");
        }
        updateActions();
        scheduleReconnect();
    }

    button.addEventListener("click", async () => {
        button.disabled = true;
        try {
            if (!window.isSecureContext || !navigator.requestMIDIAccess) {
                throw new Error("Para editar via USB, abra o editor por localhost ou HTTPS no Chrome ou Edge.");
            }
            const midi = await navigator.requestMIDIAccess({ sysex: true });
            if (!midi.sysexEnabled) throw new Error("Autorize o acesso a mensagens MIDI SysEx.");
            access = midi;
            access.addEventListener("statechange", portsChanged);
            portsChanged();
            if (firmwareLock) return;
            report("Selecione as portas USB MIDI do SMK e vincule o teclado.");
            if (preferred.input && preferred.output && rx.value && tx.value) {
                wantLink = true;
                void openLink();
            }
        } catch (error) {
            report(error.message, "error");
            button.disabled = false;
        }
    });
    for (const select of [rx, tx]) select.addEventListener("change", () => {
        wantLink = false;
        clearTimeout(reconnectTimer);
        reconnectTimer = 0;
        disconnect("Portas alteradas. Vincule o teclado.");
    });

    function receive(event) {
        const parsed = reply(event.data);
        if (!request || parsed?.code !== request.code || parsed.serial !== request.serial) return;
        const reasons = ["", "Dados de patch inválidos.", "Transferência incompleta.",
                         "Falha de integridade na transferência do patch.", "Slot vazio, protegido ou falha de gravação.",
                         "Pare a execução e aguarde a transferência USB."];
        pollFailures = 0;
        const error = parsed.status ? Object.assign(new Error(reasons[parsed.status] || "Comando recusado pelo SMK."),
            { firmwareStatus: parsed.status }) : null;
        request.finish(error, parsed.data);
    }

    function send(code, bytes = []) {
        return new Promise((resolve, reject) => {
            if (!output || request) {
                reject(new Error("Vínculo USB indisponível."));
                return;
            }
            const id = serial = (serial + 1) & 0x3fff;
            const wait = setTimeout(() => finish(Object.assign(new Error("O SMK não respondeu à consulta USB."),
                { timeout: true })), 1200);
            function finish(error, data) {
                clearTimeout(wait);
                request = undefined;
                if (error) reject(error);
                else resolve(data);
            }
            request = { code, serial: id, finish };
            try { output.send(message(code, id, bytes)); }
            catch (error) { finish(error); }
        });
    }

    link.addEventListener("click", () => {
        if (input || wantLink) {
            wantLink = false;
            clearTimeout(reconnectTimer);
            reconnectTimer = 0;
            disconnect("SMK desvinculado.");
            return;
        }
        if (firmwareLock || busy || !access || !rx.value || !tx.value) return;
        wantLink = true;
        void openLink();
    });

    async function openLink() {
        if (firmwareLock || !wantLink || busy || !access) return;
        const selectedInput = access.inputs.get(rx.value), selectedOutput = access.outputs.get(tx.value);
        if (!selectedInput || !selectedOutput) return;
        busy = true;
        updateActions();
        report("Abrindo conexão USB MIDI…", "busy");
        try {
            await Promise.all([selectedInput.open(), selectedOutput.open()]);
            if (rx.value !== selectedInput.id || tx.value !== selectedOutput.id) throw new Error("Seleção de portas alterada.");
            input = selectedInput;
            output = selectedOutput;
            input.addEventListener("midimessage", receive);
            const model = await send(command.hello);
            if (model.length !== 4 || model[0] !== 1 || model[1] !== 98 || model[3] < 6 || model[3] > 96) {
                wantLink = false;
                throw new Error("Firmware incompatível com este editor.");
            }
            openingFailures = 0;
            savePortSelection();
            partSize = model[3];
            supportsPerformance = model[1] === 98 && model[2] >= 3;
            scaleCount = supportsPerformance && model[2] === 3 ? 6 : ScaleCatalog.length;
            advancedArp = supportsPerformance && model[2] >= 5;
            supportsXY = supportsPerformance && model[2] >= 6;
            supportsExtended = supportsPerformance && model[2] >= 7;
            supportsStepTelemetry = supportsPerformance && model[2] >= 8;
            supportsGrid = supportsPerformance && model[2] >= 9;
            supportsSync = supportsPerformance && model[2] >= 10;
            supportsDelta = supportsSync && model[2] >= 11;
            supportsSparse = supportsDelta && model[2] >= 12;
            onPerformanceOptions?.(scaleCount, scaleCount > 6, advancedArp);
            link.textContent = "Desvincular SMK";
            if (supportsExtended) await refreshSlots();
            if (!input) return;
            telemetryReadout = supportsGrid;
            subscribe();
            report("SMK vinculado. Escolha carregar do teclado ou aplicar o patch do editor.", "linked");
        } catch (error) {
            openingFailures++;
            disconnect(error.message, "error");
        } finally {
            busy = false;
            portsChanged();
        }
    }

    async function transfer(value) {
        if (!supportsPerformance && value.sequence?.events[0]?.length === 4)
            throw new Error("Instale o Felucca Pro 3.1 para enviar gravações KS com notas sobrepostas.");
        if (!supportsXY && !value.ksMatrix && value.pins.some(([row]) => row === 15))
            throw new Error("Stick Y requer a Felucca Pro 3.5.");
        if (!supportsExtended && value.usbRoutes?.length) throw new Error("As entradas USB independentes exigem o Felucca Pro 3.8.");
        await writeWire(patch(value, value.ks, supportsXY, supportsExtended));
    }

    async function writeWire(bytes, grid = false) {
        await send(command.begin, [...u14(bytes.length), ...u21(crc(bytes)), ...(grid ? [1] : [])]);
        for (let offset = 0; offset < bytes.length; offset += partSize) {
            await send(command.part, [...u14(offset), ...bytes.slice(offset, offset + partSize)]);
        }
        await send(command.commit);
    }

    function subscribe() {
        if (!supportsPerformance || !(live || telemetryReadout) || !performanceState || telemetryTimer) return;
        telemetryTimer = setTimeout(() => {
            telemetryTimer = 0;
            if (!busy && !jobs.size) queue("performance-status");
            subscribe();
        }, 180);
    }

    async function followHardware() {
        if (supportsSparse) {
            if (!live || jobs.size || readPending) return;
            try {
                const next = await fetchDiff(revision);
                if (!next || jobs.size || readPending) return;
                if (mirror) onDelta?.({ previous: mirror.value, current: next.value });
                else if (!onRemotePatch?.(next.value, false)) throw new Error("Estado recebido do SMK inválido.");
                acceptDiff(next);
            } catch (error) {
                if (error.timeout) throw error;
                revision = 0;
            }
            return;
        }
        if (onPanel) {
            const state = await send(command.panel, supportsDelta ? [1] : []);
            if (state.length !== (supportsDelta ? 40 : 32)) throw new Error("Estado do painel incompleto.");
            if (!jobs.size) onPanel(Array.from({ length: state.length / 2 }, (_, index) => read14(state.slice(2 * index))), live);
        }
        if (!live || !onRemotePatch || jobs.size || readPending) return;
        if (supportsDelta) {
            const state = await send(command.sync, u21(revision));
            if (state.length < 4) throw new Error("Estado de sincronização incompleto.");
            const next = read21(state.slice(1));
            if (state[0] === 0 && state.length === 4 && next === revision) return;
            if (jobs.size || readPending) return;
            if (state[0] === 1 && revision && next && state.length > 4) {
                const delta = readDelta(state.slice(4));
                if (!delta) throw new Error("Alterações do SMK inválidas.");
                onDelta?.(delta);
            } else if (state[0] === 2 && next && state.length === 10) {
                const value = unpack(await readWire(state.slice(4)));
                if (jobs.size || readPending) return;
                if (!value || !onRemotePatch(value, Boolean(state[9]))) throw new Error("Estado recebido do SMK inválido.");
            } else throw new Error("Resposta de sincronização inválida.");
            revision = next;
            return;
        }
        const fingerprint = () => {
            const local = snapshot();
            const bytes = patch(local, local.ks, true, true);
            return [...u14(bytes.length), ...u21(crc(bytes))];
        };
        const actual = fingerprint();
        const known = actual.every((byte, index) => lastSync?.local[index] === byte) ? lastSync.remote : actual;
        const header = await send(command.read, [3, ...known]);
        if (header.length !== 6) throw new Error("Cabeçalho de sincronização inválido.");
        if (!read14(header)) return;
        const value = unpack(await readWire(header));
        if (!value) throw new Error("Estado recebido do SMK inválido.");
        if (jobs.size || readPending) return;
        if (!onRemotePatch(value, Boolean(header[5]))) throw new Error("Estado recebido do SMK inválido.");
        lastSync = { local: fingerprint(), remote: header.slice(0, 5) };
    }

    async function fetchDiff(known) {
        const state = await send(command.diff, u21(known));
        if (state.length < 4) throw new Error("Estado de sincronização incompleto.");
        const next = read21(state.slice(1));
        if (state[0] === 0 && next === known && state.length === 4) return null;
        if (!next || ![1, 2].includes(state[0]) || state.length < 9) throw new Error("Revisão inválida.");
        const header = state.slice(4, 9);
        const bytes = state.length === 9 ? await readWire(header, command.diff, 7000) : state.slice(9);
        if (bytes.length !== read14(header) || crc(bytes) !== read21(header.slice(2))) throw new Error("Alteração USB incompleta.");
        const expanded = applyDiff(state[0], bytes, mirror);
        if (!expanded) throw new Error("Alteração USB inválida.");
        return { ...expanded, revision: next };
    }

    function acceptDiff(next) {
        const changedPanel = !mirror || next.panel.some((value, index) => value !== mirror.panel[index]);
        mirror = next;
        revision = next.revision;
        seenEdits.clear();
        if (changedPanel) onPanel?.(next.panel, live);
    }

    async function sendChanges(group, values, known = []) {
        const changes = values.flatMap(([index, value]) => {
            const key = group + ":" + index;
            return value === (seenEdits.get(key) ?? known[index]) ? [] : [index, ...u14(value)];
        });
        if (!changes.length) return;
        await send(command.fields, [group, ...changes]);
        for (let i = 0; i < changes.length; i += 3)
            seenEdits.set(group + ":" + changes[i], read14(changes.slice(i + 1, i + 3)));
    }

    function groupValues(group, value) {
        if (!value) return [];
        const { ks, performance, stepGrid } = value;
        if (group === 0) return [ks.realtimeSpread, ks.sequenceSpread, ks.speed, ks.transpose + 24,
            ks.limitSeconds, Number(ks.loop), Number(value.sequenceTrigger)];
        if (group === 1) return [Number(performance.enabled), Number(performance.hold), Number(performance.freeRun),
            performance.bpm, performance.gate, performance.order, performance.division, performance.chance, performance.root, performance.scale];
        return [stepGrid.length, stepGrid.bpm, stepGrid.timingBpm, stepGrid.division];
    }

    function changed(key, value, prior) {
        return !supportsSparse || value !== (seenEdits.get(key) ?? prior);
    }

    async function flush() {
        timer = 0;
        if (busy || !(live || telemetryReadout)) return;
        busy = true;
        let edited = false, activeKey;
        try {
            while ((live || telemetryReadout) && jobs.size) {
                const [key, data] = jobs.entries().next().value;
                activeKey = key;
                jobs.delete(key);
                edited ||= key !== "performance-status";
                if (key === "patch") {
                    report("Enviando patch ao SMK…", "busy");
                    await transfer(data);
                    patchPending = jobs.has("patch");
                    if (supportsSparse) {
                        revision = 0;
                        mirror = undefined;
                        seenEdits.clear();
                    }
                } else if (key === "scalar-edits") {
                    for (const [group, fields] of data) await sendChanges(group, [...fields], groupValues(group, mirror?.value));
                } else if (key === "controls") {
                    const entries = Object.entries(data).filter(([id, value]) =>
                        changed("knob:" + id, Math.fround(value), Math.fround(mirror?.value.controls[id])));
                    const bytes = edits(Object.fromEntries(entries));
                    for (let i = 0; i < bytes.length; i += 90) await send(command.controls, bytes.slice(i, i + 90));
                    if (supportsSparse) for (const [id, value] of entries) seenEdits.set("knob:" + id, Math.fround(value));
                } else if (key === "step-edits" || key === "usb-edits" || key === "cc-edits") {
                    for (const [index, payload] of data) {
                        if (supportsSparse && key !== "step-edits") {
                            const prior = key === "cc-edits" ? mirror?.value.midiCC[index] :
                                mirror && (mirror.value.usbRoutes.find(([ch, col]) => ch * 16 + col === index)?.[2] ?? 0);
                            const field = key + ":" + index, value = payload.at(-1);
                            if (!changed(field, value, prior)) continue;
                            await send(key === "cc-edits" ? command.midiCC : command.usbRoute, payload);
                            seenEdits.set(field, value);
                            continue;
                        }
                        await send(key === "step-edits" ? command.step : key === "usb-edits" ? command.usbRoute : command.midiCC, payload);
                    }
                } else if (key === "panel-edits") {
                    for (const [index, value] of data) {
                        if (!changed("panel:" + index, value, mirror?.panel[index])) continue;
                        await send(command.panel, [index, ...u14(value)]);
                        if (supportsSparse) seenEdits.set("panel:" + index, value);
                    }
                } else if (key === "ks") await send(command.ks, ks(data));
                else if (key === "transport") await send(command.transport, [data]);
                else if (key === "performance-status") {
                    const section = supportsStepTelemetry ? 7 : advancedArp ? 6 : 4;
                    const state = await send(command.performance, supportsSparse && lastTelemetry !== undefined
                        ? [section, ...u21(lastTelemetry)] : [section]);
                    if (status.dataset.link === "retry") report(live ? "Edição ao vivo via USB." : "SMK vinculado.", live ? "live" : "linked");
                    if (state.length === (supportsStepTelemetry ? 24 : advancedArp ? 20 : 18) &&
                        !jobs.has("arp-settings") && !jobs.has("arp-modes") && !jobs.has("scale-settings")) performanceState({
                        editing: live, sparse: supportsSparse,
                        mode: ["stop", "record", "play"][state[0]],
                        count: read14(state.slice(1)), limit: ["", "time", "memory"][state[3]],
                        position: read21(state.slice(4)) / 1000, duration: read21(state.slice(7)) / 1000,
                        arp: { enabled: Boolean(state[10] & 1), bpm: read14(state.slice(11)), gate: state[13], order: state[14], division: scaleCount > 6 ? (state[10] >> 2) & 7 : 2,
                            hold: Boolean(state[18] & 1), freeRun: Boolean(state[18] & 2), chance: state[19] ?? 100 },
                        scale: { root: state[15], mode: state[16] },
                        arpNote: state[10] & 2 ? state[17] : null,
                        step: advancedArp ? {
                            recording: Boolean(state[18] & 4), playing: Boolean(state[18] & 8),
                            index: supportsStepTelemetry ? state[20] : null,
                            clock: supportsStepTelemetry ? {
                                external: Boolean(state[21] & 1), locked: Boolean(state[21] & 2), bpm: read14(state.slice(22))
                            } : null
                        } : null
                    });
                    if (supportsSparse && state.length === 24) lastTelemetry = crc(state);
                    if (supportsSync && !jobs.size && !readPending && Date.now() >= syncAfter) {
                        syncAfter = Date.now() + 300;
                        await followHardware();
                    }
                } else if (key === "ks-routes") {
                    for (const [index, payload] of data) {
                        const field = "ks-route:" + index, value = payload[2];
                        if (!changed(field, value, mirror && (mirror.value.ksRoutes.find(([col]) => col === index)?.[1] ?? 0))) continue;
                        await send(command.performance, payload);
                        if (supportsSparse) seenEdits.set(field, value);
                    }
                } else if (key === "attack" || key === "stick-x" || key === "stick-y" || key === "ks-matrix" ||
                           key === "arp-settings" || key === "arp-modes" || key === "scale-settings") {
                    await send(command.performance, data);
                }
                else if (key === "pins") {
                    const entries = [...data].filter(([index, [, , value]]) => changed("pin:" + index, value,
                        mirror && (mirror.value.pins.find(([row, col]) => row * 16 + col === index)?.[2] ?? 0)));
                    const bytes = entries.flatMap(([, payload]) => payload);
                    for (let i = 0; i < bytes.length; i += 90) await send(command.pin, bytes.slice(i, i + 90));
                    if (supportsSparse) for (const [index, [, , value]] of entries) seenEdits.set("pin:" + index, value);
                }
            }
            if (live && edited) report("Edição ao vivo · mudanças confirmadas pelo SMK", "live");
        } catch (error) {
            if (error.firmwareStatus) {
                if (activeKey === "patch") {
                    live = false;
                    jobs.clear();
                    patchPending = false;
                    apply.textContent = "Aplicar patch e editar ao vivo";
                }
                report(error.message, "error");
            } else if (activeKey === "performance-status" && error.timeout && ++pollFailures < 3) {
                report("Aguardando resposta do SMK. A conexão USB está aberta.", "retry");
            } else {
                disconnect(error.message + " Tentando restabelecer o vínculo USB.", "error");
            }
        } finally {
            finishAction();
        }
    }

    function finishAction() {
        busy = false;
        updateActions();
        scheduleReconnect();
        if (jobs.size && !timer) timer = setTimeout(flush, 25);
        if (readPending && !jobs.size) void readPatch();
    }

    function queue(key, data) {
        if (!live && key !== "performance-status") return;
        if (key === "patch") {
            patchPending = true;
            updateActions();
            for (const update of ["patch", "controls", "pins", "ks", "transport", "ks-routes", "ks-matrix", "step-edits", "usb-edits", "cc-edits", "scalar-edits"]) jobs.delete(update);
        }
        if (key === "controls") data = { ...jobs.get(key), ...data };
        jobs.set(key, data);
        if (!busy && !timer) timer = setTimeout(flush, 25);
    }
    apply.addEventListener("click", () => {
        if (!input || patchPending || readPending) return;
        live = true;
        lastTelemetry = undefined;
        apply.textContent = "Reenviar patch completo";
        queue("patch", snapshot());
        subscribe();
    });
    async function readPatch() {
        if (!input) return;
        readPending = true;
        updateActions();
        if (busy || jobs.size) return;
        busy = true;
        const wasLive = live;
        live = false;
        report("Lendo patch do SMK…", "busy");
        try {
            revision = 0;
            lastTelemetry = undefined;
            if (supportsSparse) {
                const next = await fetchDiff(0);
                if (next) {
                    if (!loadPatch(next.value)) throw new Error("O patch recebido está inválido.");
                    live = true;
                    acceptDiff(next);
                    subscribe();
                    report("Patch do SMK carregado · edição ao vivo via USB", "live");
                    apply.textContent = "Reenviar patch completo";
                    return true;
                }
            }
            let header, baseline = 0;
            if (supportsDelta && !supportsSparse) {
                const state = await send(command.sync, u21(0));
                if (state[0] === 2 && state.length === 10) {
                    baseline = read21(state.slice(1));
                    header = state.slice(4);
                }
            }
            header ??= await send(command.read, supportsExtended ? [2] : supportsXY ? [1] : []);
            if (header.length !== 6) throw new Error("Cabeçalho do patch inválido.");
            const value = unpack(await readWire(header));
            if (!value || !loadPatch(value)) throw new Error("O patch recebido está inválido.");
            revision = baseline;
            live = true;
            subscribe();
            report(header[5]
                ? "Patch recebido · encerre a gravação KS e carregue novamente para incluí-la."
                : "Patch do SMK carregado · edição ao vivo via USB", "live");
            apply.textContent = "Reenviar patch completo";
            return true;
        } catch (error) {
            if (error.timeout) disconnect(error.message + " Tentando restabelecer o vínculo USB.", "error");
            else {
                live = wasLive && Boolean(input);
                report(error.message, "error");
            }
        } finally {
            readPending = false;
            finishAction();
        }
    }

    async function readWire(header, partCommand = command.readPart, limit = 3000) {
        const size = read14(header);
        if (!size || size > limit) throw new Error("Tamanho de transferência inválido.");
        const bytes = [];
        for (let offset = 0; offset < size; offset += partSize) {
            const part = await send(partCommand, u14(offset));
            if (part.length !== Math.min(partSize, size - offset)) throw new Error("Leitura incompleta.");
            bytes.push(...part);
        }
        if (crc(bytes) !== read21(header.slice(2))) throw new Error("Falha de integridade na leitura.");
        return bytes;
    }

    function setMatrixSlots(occupied) {
        if (!matrixPreset) return;
        for (const option of matrixPreset.options) {
            if (!/^[0-2]$/.test(option.value)) continue;
            const slot = Number(option.value);
            option.disabled = !occupied[slot];
            option.textContent = slotLabel(slot) + (occupied.length && !occupied[slot] ? " · vazio" : "");
            if (option.disabled && matrixPreset.value === option.value) matrixPreset.value = "active";
        }
    }

    async function refreshSlots() {
        const occupied = await send(command.user, [0]);
        if (occupied.length === 3) setMatrixSlots(occupied);
        if (occupied.length === 3 && userSlot?.options.length === 3) {
            for (let i = 0; i < occupied.length; i++)
                userSlot.options[i].textContent = "Slot " + (i + 1) + (occupied[i] ? " · ocupado" : " · vazio");
        }
        return occupied;
    }

    function slotLabel(index) {
        return "USER " + (index + 1);
    }

    async function deviceAction(action) {
        if (!input || busy || writePending() || patchPending || readPending) {
            report("Aguarde a operação USB em andamento.", "error");
            return;
        }
        jobs.delete("performance-status");
        busy = true;
        updateActions();
        try {
            return await action();
        } catch (error) {
            if (error.timeout) disconnect(error.message + " Tentando restabelecer o vínculo USB.", "error");
            else report(error.message, "error");
        } finally {
            finishAction();
        }
    }

    read.addEventListener("click", readPatch);
    async function useSlot(mode, slot = Number(userSlot.value)) {
        if (!supportsExtended) return;
        const done = await deviceAction(async () => {
            if (mode === "read") {
                await send(command.user, [1, slot]);
            } else {
                const occupied = await refreshSlots();
                if (mode === "sequence" && !occupied[slot]) throw new Error("Grave primeiro um preset nesse slot USER.");
                if (occupied[slot] && !window.confirm((mode === "sequence" ? "Enviar o SEQ do editor e substituir a sequência em " :
                    "Substituir o preset em ") + slotLabel(slot) + "?")) return false;
                if (mode === "editor") await transfer(snapshot());
                if (mode === "sequence") await writeWire(ZiviStudioProtocol.encodeGrid(snapshot().stepGrid), true);
                await send(command.user, [mode === "sequence" ? 3 : 2, slot]);
                await refreshSlots();
                report(slotLabel(slot) + " gravado na memória do SMK.", live ? "live" : "linked");
            }
            return true;
        });
        if (done && mode === "read") return readPatch();
    }
    userRead?.addEventListener("click", () => useSlot("read"));
    matrixPreset?.addEventListener("change", updateActions);
    matrixForm?.addEventListener("submit", async (event) => {
        event.preventDefault();
        if (matrixLoad.disabled) return;
        const choice = matrixPreset.value;
        if (choice.startsWith("factory:")) {
            matrixLoading = true;
            updateActions();
            try { await loadLocalPreset?.(choice.slice(8), matrixFeedback); }
            finally {
                matrixLoading = false;
                updateActions();
            }
            return;
        }
        if (!input) return;
        if (writePending()) {
            if (matrixFeedback) matrixFeedback.textContent = "Aguarde a operação USB em andamento.";
            return;
        }
        const slot = Number(choice);
        if (choice !== "active" && ![...matrixPreset.options].some(option => option.value === choice && !option.disabled)) return;
        matrixLoading = true;
        updateActions();
        if (matrixFeedback) matrixFeedback.textContent = "";
        try {
            const loaded = choice === "active" ? await readPatch() : await useSlot("read", slot);
            if (matrixFeedback && input) {
                matrixFeedback.textContent = loaded ? "Patch recebido do SMK." : readPending ? "" : status.textContent;
            }
        } finally {
            matrixLoading = false;
            updateActions();
        }
    });
    userSave?.addEventListener("click", () => useSlot("active"));
    userWrite?.addEventListener("click", () => useSlot("editor"));
    stepStore?.addEventListener("click", () => useSlot("sequence"));
    stepRead?.addEventListener("click", () => {
        if (!supportsGrid) return;
        void deviceAction(async () => {
            const header = await send(command.step, [4]);
            if (header.length !== 5) throw new Error("Cabeçalho do SEQ inválido.");
            const grid = ZiviStudioProtocol.decodeGrid(await readWire(header));
            if (!grid || !onGrid?.(grid)) throw new Error("O SEQ recebido está inválido.");
            report("SEQ recebido do SMK.", live ? "live" : "linked");
        });
    });
    stepWrite?.addEventListener("click", () => {
        if (!supportsGrid) return;
        void deviceAction(async () => {
            await writeWire(ZiviStudioProtocol.encodeGrid(snapshot().stepGrid), true);
            report("SEQ enviado ao SMK.", live ? "live" : "linked");
        });
    });
    function readProfile(reset) {
        if (!supportsDelta) return;
        void deviceAction(async () => {
            const data = await send(command.profile, reset ? [1] : []);
            if (data.length !== 40) throw new Error("Diagnóstico do SMK incompleto.");
            const [blocks, peak, late, depth, miss, dma, drop, studioPeak] =
                Array.from({ length: 8 }, (_, index) => read32(data, index * 5));
            get("profile-result").textContent = [
                blocks + " blocos", "DSP máx. " + peak + " µs", late + " acima de 2,90 ms",
                "Fila mín. " + (depth === 0xffffffff ? "—" : depth), "MISS +" + miss, "DMA +" + dma,
                "USB DROP +" + drop, "Studio máx. " + studioPeak + " µs"
            ].join(" · ");
        });
    }
    profileRead?.addEventListener("click", () => readProfile(false));
    profileReset?.addEventListener("click", () => readProfile(true));
    updateActions();
    function queueScalars(group, pairs) {
        const updates = jobs.get("scalar-edits") ?? new Map();
        const fields = updates.get(group) ?? new Map();
        for (const [index, value] of pairs) fields.set(index, value);
        updates.set(group, fields);
        queue("scalar-edits", updates);
    }
    const edit = (type, values) => {
        const stepTransport = type === "step-play" || type === "step-record";
        if (stepTransport && supportsGrid && !live) {
            void deviceAction(() => send(command.step, type === "step-record" ? [3] : [2, Number(values[0])]));
            return true;
        }
        if (!live) return;
        if (type === "panel-config" && supportsSync && (values[0] < 16 || supportsDelta && values[0] < 18 || supportsSparse && values[0] === 20)) {
            const edits = jobs.get("panel-edits") ?? new Map();
            edits.set(...values);
            queue("panel-edits", edits);
        }
        if (type === "patch") queue("patch", snapshot());
        if (type === "controls") queue("controls", values[0]);
        if (type === "step-cell" || type === "step-grid" || type === "step-play" || type === "step-record" || type === "usb-route" || type === "midi-cc") {
            if (!supportsExtended) {
                report("Esta edição requer o Felucca Pro 3.8.", "error");
                return live;
            }
            const key = type === "usb-route" ? "usb-edits" : type === "midi-cc" ? "cc-edits" : "step-edits";
            const updates = jobs.get(key) ?? new Map();
            if (type === "midi-cc") updates.set(values[0], values);
            if (type === "usb-route") updates.set(values[0] * 16 + values[1], values);
            if (type === "step-cell") updates.set(values[0], [1, ...values]);
            if (type === "step-grid" && supportsSparse) {
                queueScalars(2, [[0, values[0]], [1, values[1]], [3, values[2]]]);
                return live;
            }
            if (type === "step-grid") updates.set("grid", [0, values[0], ...u14(values[1]), values[2]]);
            if (type === "step-play") updates.set("transport", [2, Number(values[0])]);
            if (type === "step-record") updates.set("transport", [3]);
            queue(key, updates);
        }
        if (type === "pin") {
            if (!supportsXY && values[0] === 15 && !snapshot().ksMatrix) {
                report("Stick Y requer a Felucca Pro 3.5.", "error");
                return live;
            }
            const pins = jobs.get("pins") ?? new Map();
            pins.set(16 * values[0] + values[1], values);
            queue("pins", pins);
        }
        if (type === "ks-controls") {
            const k = values[0];
            if (supportsSparse) queueScalars(0, [k.realtimeSpread, k.sequenceSpread, k.speed, k.transpose + 24,
                k.limitSeconds, Number(k.loop), Number(k.sequenceTrigger)].map((value, index) => [index, value]));
            else queue("ks", k);
        }
        if (type === "ks-stop") queue("transport", 0);
        if (type === "ks-play") queue("transport", 1);
        if (type === "ks-record") queue("transport", 2);
        if (supportsPerformance && type === "arp-settings") {
            const { enabled, bpm, gate, order, division = 2, hold = false, freeRun = false, chance = 100 } = values[0];
            if (supportsSparse) queueScalars(1, [Number(enabled), Number(hold), Number(freeRun), bpm, gate, order, division, chance]
                .map((value, index) => [index, value]));
            else {
                const payload = [2, Number(enabled), ...u14(bpm), gate, advancedArp || order < 3 ? order : 0];
                if (scaleCount > 6) payload.push(division);
                queue(type, payload);
                if (advancedArp) queue("arp-modes", [5, Number(hold) | (Number(freeRun) << 1), chance]);
            }
        }
        if (supportsPerformance && type === "scale-settings" && values[1] < scaleCount) {
            if (supportsSparse) queueScalars(1, [[8, values[0]], [9, values[1]]]);
            else queue(type, [3, ...values]);
        }
        if (supportsPerformance && type === "manual-attack") queue("attack", [0, Number(values[0])]);
        if ((supportsPerformance && type === "stick-x") || (supportsXY && type === "stick-y")) {
            const axis = Math.max(-1, Math.min(1, supportsDelta ? values[1] ?? values[0] : values[0]));
            const position = u14(8192 + Math.round(axis * (axis >= 0 ? 8191 : 8192)));
            queue(type, supportsDelta ? [11, Number(type === "stick-y"), ...position] : [type === "stick-x" ? 1 : 8, ...position]);
        }
        if (type === "ks-route") {
            const [col, amount] = values;
            if (supportsXY) {
                const routes = jobs.get("ks-routes") ?? new Map();
                routes.set(col, [9, col, amount]);
                queue("ks-routes", routes);
            } else if (!snapshot().ksMatrix) {
                const pins = jobs.get("pins") ?? new Map();
                pins.set(240 + col, [15, col, amount]);
                queue("pins", pins);
            }
        }
        if (type === "ks-matrix") {
            if (supportsXY) queue(type, [10, Number(values[0])]);
            else queue("patch", snapshot());
        }
        return live;
    };
    edit.isEditing = () => live;
    edit.updateActions = updateActions;
    edit.pauseEditing = () => {
        if (!live) return true;
        if ((busy && request?.code !== command.performance) || patchPending || readPending || writePending()) return false;
        live = false;
        apply.textContent = "Aplicar patch e editar ao vivo";
        report("Modo Try · edição local. O SMK continua vinculado.", "linked");
        return true;
    };
    edit.lockFirmware = () => {
        if (busy || patchPending || readPending || writePending())
            throw new Error("Aguarde a transferência do Studio USB antes de gravar firmware.");
        firmwareLock = true;
        wantLink = false;
        clearTimeout(reconnectTimer);
        reconnectTimer = 0;
        disconnect("Studio USB pausado durante a gravação de firmware.");
        return () => {
            firmwareLock = false;
            updateActions();
            report("Vincule o teclado novamente para editar os patches.");
        };
    };
    return edit;
}

function setupCustomPresets(snapshot, apply, validate) {
    const select = document.getElementById("user-preset");
    const name = document.getElementById("user-name");
    const save = document.getElementById("user-save");
    const update = document.getElementById("user-update");
    const remove = document.getElementById("user-delete");
    const status = document.getElementById("patch-status");
    const storageKey = "zivisynthiA.custom.v1";
    let entries = [];
    try {
        const stored = JSON.parse(localStorage.getItem(storageKey) ?? "[]");
        if (Array.isArray(stored)) entries = stored.flatMap(entry => {
            const patch = validate(entry?.patch);
            return patch?.name && typeof entry.id === "string" ? [{ id: entry.id, patch }] : [];
        });
    } catch {
        status.textContent = "Não foi possível ler os presets deste navegador.";
    }

    function redraw(id = "") {
        select.replaceChildren(new Option("Novo preset", ""));
        for (const entry of entries) select.add(new Option(entry.patch.name, entry.id));
        select.value = id;
        update.disabled = remove.disabled = !select.value;
    }

    function persist(next, id, feedback) {
        try {
            localStorage.setItem(storageKey, JSON.stringify(next));
        } catch {
            status.textContent = "Não foi possível salvar neste navegador. Use Exportar para guardar o patch.";
            return;
        }
        entries = next;
        redraw(id);
        status.textContent = feedback;
    }

    function write(id) {
        const title = name.value.trim();
        if (!title) {
            name.focus();
            status.textContent = "Dê um nome ao preset.";
            return;
        }
        const patch = validate({ ...snapshot(), name: title });
        if (!patch) {
            status.textContent = "O patch atual contém valores inválidos.";
            return;
        }
        let next;
        if (id) next = entries.map(entry => entry.id === id ? { id, patch } : entry);
        else {
            id = globalThis.crypto?.randomUUID?.() ?? Date.now() + "-" + Math.random().toString(36).slice(2);
            next = [...entries, { id, patch }];
        }
        persist(next, id, "Preset salvo neste navegador.");
    }

    select.addEventListener("change", () => {
        const entry = entries.find(item => item.id === select.value);
        if (entry) {
            apply(entry.patch, "", true);
            name.value = entry.patch.name;
            status.textContent = "Preset carregado.";
        } else name.value = "";
        redraw(select.value);
    });
    save.addEventListener("click", () => write(""));
    update.addEventListener("click", () => { if (select.value) write(select.value); });
    remove.addEventListener("click", () => {
        const entry = entries.find(item => item.id === select.value);
        if (entry && window.confirm("Excluir o preset “" + entry.patch.name + "”?")) {
            persist(entries.filter(item => item.id !== entry.id), "", "Preset excluído.");
            name.value = "";
        }
    });
    redraw();
    return title => {
        redraw();
        name.value = title;
    };
}
