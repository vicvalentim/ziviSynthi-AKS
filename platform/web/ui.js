(() => {
    "use strict";

    const byId = id => document.getElementById(id);
    const audioButton = byId("audio");
    const audioStatus = byId("audio-status");
    const midiStatus = byId("midi-status");
    const controls = ["tune", "shape", "level"].map(byId);
    const extraControls = ["mix", "lfo", "cutoff"].map(byId);
    const moduleTitles = {
        FILTER: "Filter / Oscillator", RING: "Ring Modulator", REVERB: "Reverberation",
        ENV: "Envelope Shaper", IN: "Input Level", OUT: "Output Channels"
    };
    function rack(container, title, id, area, color) {
        const section = document.createElement("section");
        const heading = document.createElement("h3");
        const controls = document.createElement("div");
        section.id = id;
        section.className = "bank-section";
        section.dataset.color = color;
        section.dataset.area = area;
        heading.textContent = title;
        controls.className = "dial-row";
        section.append(heading, controls);
        container.append(section);
        return controls;
    }
    const panel = byId("module-controls");
    const areas = ["osc1", "osc2", "osc3", "noise", "ring", "filter", "env", "reverb", "input", "output"];
    const banks = PanelCatalog.map(({ title, color, tab }, index) =>
        rack(panel, moduleTitles[tab] ?? title, "bank-" + index, areas[index], color));
    const outputFilter = rack(panel, "Output Filter", "output-filter", "tone", "yellow");
    const digitalControls = byId("digital-controls");
    const dialById = new Map(PanelCatalog.flatMap(({ dials }) => dials.filter(Boolean).map(dial => [dial.id, dial])));
    const modes = { envMode: ["Gate", "Único", "Repetir"], triggerMode: ["Teclado / KS", "Limiar em C"] };
    const formatHz = ZiviVisuals.frequency;
    const { dialFrequency, verniers } = ZiviVisuals;
    const percent = value => Math.round(value * 100) + "%";
    const locale = document.documentElement.lang;
    const numberFormat = new Intl.NumberFormat(locale, { maximumFractionDigits: 2 });
    const decimal = value => numberFormat.format(value);
    const formatAdvanced = (id, number) => {
        if (modes[id]) return modes[id][number];
        if (id.endsWith("Hz")) return dialFrequency(number);
        if (id.endsWith("Ms")) return decimal(number) + " ms";
        switch (dialById.get(id).format) {
        case "PCT": return percent(number);
        case "SEC": return decimal(number) + " s";
        case "FACTOR": return decimal(number) + "×";
        default: return decimal(number);
        }
    };
    const advanced = ModuleCatalog.map(({ id, label, lo, hi, initial, step }) => {
        const wrapper = document.createElement(id.endsWith("Hz") ? "div" : "label");
        const caption = document.createElement("span");
        const value = document.createElement("output");
        const input = document.createElement("input");
        wrapper.className = "control";
        caption.textContent = label;
        value.setAttribute("for", id);
        input.id = id;
        input.type = "range";
        input.min = lo;
        input.max = hi;
        input.step = modes[id] ? step : "any";
        input.value = initial;
        input.setAttribute("aria-label", label);
        wrapper.append(caption, input, value);
        const knob = ZiviVisuals.knob(input, value, number => formatAdvanced(id, number),
            { vernier: id.endsWith("Hz") ? verniers.audio : undefined, taper: id === "response" ? ZiviVisuals.responseTaper : undefined,
              log: id.endsWith("Ms") && lo > 0, increment: step });
        return { input, knob };
    });
    const quickFormat = {
        tune: value => (value > 0 ? "+" : "") + decimal(value) + " st",
        shape: percent, level: percent, mix: percent, lfo: dialFrequency, cutoff: dialFrequency
    };
    const knobs = new Map([...controls, ...extraControls].map(input => [
        input.id, ZiviVisuals.knob(input, byId(input.id + "-value"), quickFormat[input.id],
            { vernier: input.id === "lfo" ? verniers.low : input.id === "cutoff" ? verniers.filter : undefined,
              increment: input.id === "tune" ? 0.5 : 0.01 })
    ]));
    advanced.forEach(({ input, knob }) => knobs.set(input.id, knob));
    const allControls = [...controls, ...extraControls, ...advanced.map(entry => entry.input)];
    const parameters = Object.fromEntries(allControls.map(input => [input.id, input]));
    parameters.cutoff.title = "Posição 5 · referência de 261 Hz em auto-oscilação com Response no máximo.";
    parameters.response.title = "A posição 5 indica a região de início da auto-oscilação.";
    const panelExtras = new Set(["tune", "mix", "level", "filterDrive", "envMode", "triggerMode", "reverbTime"]);
    const frontLabels = {
        osc1Hz: "Frequency", lfo: "Frequency", osc2Hz: "Frequency", cutoff: "Frequency",
        shape: "Shape", osc2Shape: "Shape", osc3Shape: "Shape",
        sineLevel: "Sine", rampLevel: "Ramp", osc2Pulse: "Pulse", osc2Tri: "Triangle",
        osc3Pulse: "Pulse", osc3Tri: "Triangle", out1Tone: "Channel 1", out2Tone: "Channel 2",
        out1Level: "Level 1", out2Level: "Level 2", out1Pan: "Pan 1", out2Pan: "Pan 2",
        level: "Nível do patch", tune: "Afinação fina", mix: "Blend OSC 1",
        envMode: "Modo ENV", triggerMode: "Disparo ENV", reverbTime: "Tempo da mola"
    };
    const frontColors = { cutoff: "blue", response: "yellow", filterLevel: "white",
        out1Tone: "yellow", out2Tone: "yellow", in1Level: "white", in2Level: "white" };
    const largeDials = new Set(["osc1Hz", "osc2Hz", "lfo"]);
    const outputOrder = ["out1Level", "out1Pan", "out2Pan", "out2Level"];
    PanelCatalog.forEach(({ dials }, page) => dials.forEach((dial, index) => {
        if (!dial) return;
        const { id } = dial;
        const wrapper = parameters[id].closest(".control");
        wrapper.dataset.color = frontColors[id] ?? dial.color;
        if (largeDials.has(id)) wrapper.classList.add("frequency-dial");
        wrapper.querySelector("span").textContent = frontLabels[id] ?? dial.label;
        const physical = document.createElement("span");
        physical.className = "encoder";
        physical.textContent = "K" + (index + 1);
        physical.title = "Controle físico no SMK-37 Pro";
        wrapper.prepend(physical);
        if (panelExtras.has(id)) digitalControls.append(wrapper);
        else if (id === "out1Tone" || id === "out2Tone") outputFilter.append(wrapper);
        else banks[page].append(wrapper);
    }));
    const outputRack = banks[areas.indexOf("output")];
    for (const id of outputOrder) outputRack.append(parameters[id].closest(".control"));
    const envelopeRack = banks[areas.indexOf("env")];
    for (const id of ["trapLevel", "envLevel"]) envelopeRack.append(parameters[id].closest(".control"));
    byId("module-quick-controls").remove();
    for (const link of document.querySelectorAll("[data-open-panel]")) link.addEventListener("click", () => {
        const panel = byId(link.hash.slice(1));
        if (panel) panel.open = true;
    });

    if (location.hash === "#firmware-drawer") byId("firmware-drawer").open = true;
    function openLinkedKsEditor() {
        if (location.hash === "#ks-editor") {
            if (byId("seq-ks").hidden) document.querySelector('[data-seq="ks"]').click();
            byId("ks-editor").open = true;
        }
    }
    openLinkedKsEditor();
    window.addEventListener("hashchange", openLinkedKsEditor);

    const exactSelect = byId("exact-param"), exactValue = byId("exact-value");
    for (const input of allControls)
        exactSelect.add(new Option(input.getAttribute("aria-label") || input.id, input.id));
    function updateExactValue() {
        const input = parameters[exactSelect.value];
        if (!input) return;
        exactValue.min = input.min;
        exactValue.max = input.max;
        exactValue.step = input.step;
        exactValue.value = input.value;
    }
    exactSelect.addEventListener("change", updateExactValue);
    byId("exact-edit").addEventListener("submit", event => {
        event.preventDefault();
        const input = parameters[exactSelect.value];
        if (!input || !exactValue.reportValidity()) return;
        input.value = exactValue.value;
        input.dispatchEvent(new Event("input"));
    });
    updateExactValue();
    const { sources, destinations, supported, delayed, coordinate, presets, validatePatch } = ZiviDSP;
    for (const [key, preset] of Object.entries(FactoryCatalog)) {
        const option = document.createElement("option");
        option.value = key;
        option.textContent = preset.name;
        byId("factory-" + preset.bank).append(option);
    }
    const matrixPreset = byId("smk-matrix-preset");
    for (const group of document.querySelectorAll("#preset optgroup")) {
        const factoryGroup = group.cloneNode(true);
        factoryGroup.removeAttribute("id");
        for (const option of factoryGroup.querySelectorAll("option")) option.value = "factory:" + option.value;
        matrixPreset.append(factoryGroup);
    }
    const presetGuide = byId("preset-guide");
    let portableSequence, hardwarePerformance, ksState, ksFromGrid = false;
    let ksRevision = 0;
    const ksTrigger = byId("ks-trigger");
    let arpEnabled = false;
    const chromaticNames = ["C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "A♯", "B"];
    let patchName;
    let resetCustomSelection = () => {};
    const cells = new Map();
    const columnLabels = [], rowLabels = [];
    const pins = new Uint8Array(256), ksRoutes = new Uint8Array(16);
    const pinColors = new Map();
    const pinTypes = {
        white: { label: "Branco", amount: 1 },
        red: { label: "Vermelho", amount: 1 },
        green: { label: "Verde", amount: 2 }
    };
    const pinColor = index => pins[index] === 2 ? "green" : pins[index] === 1 ? pinColors.get(index) ?? "white" : "";
    const pinStatus = byId("pin-status");
    const pinButtons = [...document.querySelectorAll("[data-pin-type]")];
    const stock = byId("pin-stock");
    const pinStock = Array.from({ length: 21 }, () => document.createElement("span"));
    stock.append(...pinStock);
    let selectedPin = "white";
    let pinsInUse = 0;
    const ksMatrix = byId("ks-matrix");
    const usbRoutes = new Uint8Array(32);
    const usbRouteControls = new Map();
    let stepGrid = ZiviStudioProtocol.emptyGrid(), activeStep = 0, pendingTryGrid, playingStepGrid = false;
    let midiCC = ZiviStudioProtocol.defaultCC();
    const regionButtons = [];
    let selectedRow = 11;
    let selectedCol = 0;
    let zoom = false;
    let muted = false;
    const inputNotes = new Map();
    const midiPedals = new Set(), sustainedOwners = new Set();
    let previewMidiEdit = false;
    const keys = new Map();
    const typing = "awsedftgyhujk";
    const names = ["C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "B♭", "B", "C"];
    let midi;
    let audioContext;
    let audioStart;
    let processor;
    let splitter;
    let analyser;
    let telemetry;
    const panelMeter = byId("monitor-strip");
    const meterCv = byId("meter-cv");
    const meterReading = byId("meter-reading");
    const meterNeedle = byId("vu-needle");
    let phase = 0;
    let scopeActive = true;
    const waveBuffer = new Float32Array(2048);
    const spectrumBuffer = new Float32Array(waveBuffer.length / 2);
    const signalDisplay = byId("signal-display");
    const envelopeDisplay = byId("envelope-display");
    const scopeMode = byId("trace-mode");
    const scopeChannel = byId("trace-channel");
    const scopeToggle = byId("scope-toggle");
    const frequencyOutputs = [1, 2, 3, 4].map(number => byId("freq-" + number));

    function updatePanelMeter() {
        const cv = meterCv.checked;
        const value = cv ? telemetry?.matrixCv ?? 0 : telemetry?.matrix ?? 0;
        const percent = Math.round(value * 100);
        panelMeter.dataset.mode = cv ? "cv" : "signal";
        meterReading.value = (cv && percent > 0 ? "+" : "") + percent + "%";
        meterNeedle.style.setProperty("--needle", (cv ? value * 52 : value * 104 - 52) + "deg");
    }

    meterCv.addEventListener("change", updatePanelMeter);
    byId("meter-signal").addEventListener("change", updatePanelMeter);
    updatePanelMeter();

    let liveEdit = () => {};
    function send(type, ...values) {
        const hardware = !previewMidiEdit && liveEdit(type, values);
        processor?.port.postMessage({ type, values });
        return hardware;
    }

    function drawEnvelope() {
        byId("envelope-scale").textContent = ZiviVisuals.envelope(envelopeDisplay, {
            mode: parameters.envMode.valueAsNumber, attack: parameters.attackMs.valueAsNumber,
            on: parameters.onMs.valueAsNumber, decay: parameters.decayMs.valueAsNumber,
            off: parameters.offMs.valueAsNumber
        }, phase);
    }

    function drawSignal() {
        const mode = scopeMode.value;
        const rate = audioContext?.sampleRate ?? 48000;
        const data = analyser && mode !== "filter" ? (mode === "wave" ? waveBuffer : spectrumBuffer) : null;
        if (data === waveBuffer) analyser.getFloatTimeDomainData(data);
        if (data === spectrumBuffer) analyser.getFloatFrequencyData(data);
        byId("signal-scale").textContent = ZiviVisuals.signal(signalDisplay, mode, rate, data, {
            cutoff: frequencyOutputs[3].dataset.modulated === "true" && telemetry?.hz?.[3]
                ? telemetry.hz[3] : parameters.cutoff.valueAsNumber,
            response: parameters.response.valueAsNumber, drive: parameters.filterDrive.valueAsNumber,
            level: parameters.filterLevel.valueAsNumber
        });
    }

    function connectAnalyser() {
        splitter?.disconnect();
        const filterMode = scopeMode.value === "filter";
        scopeChannel.disabled = filterMode;
        if (splitter && scopeActive && !filterMode) splitter.connect(analyser, Number(scopeChannel.value));
        if (scopeActive) drawSignal();
    }

    scopeMode.addEventListener("change", connectAnalyser);
    scopeChannel.addEventListener("change", connectAnalyser);
    scopeToggle.addEventListener("click", () => {
        scopeActive = !scopeActive;
        scopeToggle.setAttribute("aria-pressed", String(scopeActive));
        scopeToggle.textContent = scopeActive ? "Pausar" : "Retomar";
        scopeMode.disabled = !scopeActive;
        connectAnalyser();
        scopeChannel.disabled = !scopeActive || scopeMode.value === "filter";
    });

    const controlValues = () => Object.fromEntries(allControls.map(input => [input.id, input.valueAsNumber]));
    const patchSnapshot = () => ({
        version: 3,
        ksRoutes: Array.from(ksRoutes).flatMap((amount, col) => amount ? [[col, amount]] : []),
        ksMatrix: ksMatrix.checked,
        usbRoutes: Array.from(usbRoutes).flatMap((amount, at) => amount ? [[at >> 4, at & 15, amount]] : []),
        stepGrid,
        midiCC,
        performance: { ...arpSettings(), root: Number(byId("scale-root").value), scale: Number(byId("scale-mode").value) },
        ...(portableSequence ? { sequence: portableSequence } : {}),
        ...(portableSequence && ksTrigger.value === "auto" ? { ksAutoTrigger: true } : {}),
        ...(patchName ? { name: patchName } : {}),
        ...(pinColors.size ? { pinColors: [...pinColors].map(([index, color]) => [index >> 4, index % 16, color]) } : {}),
        pins: Array.from(pins).flatMap((pin, index) => pin ? [[Math.floor(index / 16), index % 16, pin]] : []),
        controls: controlValues(),
        ks: readKsSettings(),
        sequenceTrigger: ksTrigger.value === "sequence"
    });

    function markEdited() {
        byId("preset").value = "";
        highlightInstantPatch("");
        if (byId("patch-status").textContent !== "") byId("patch-status").textContent = "";
    }

    const envelopeControls = new Set(["envMode", "attackMs", "onMs", "decayMs", "offMs"]);
    const filterControls = new Set(["cutoff", "response", "filterDrive", "filterLevel"]);
    allControls.forEach(input => input.addEventListener("input", () => {
        const id = input.id;
        const changes = { [id]: input.valueAsNumber };
        const repaint = [id];
        if (id === "mix") {
            changes.sineLevel = 1 - changes.mix;
            changes.rampLevel = changes.mix;
            parameters.sineLevel.value = changes.sineLevel;
            parameters.rampLevel.value = changes.rampLevel;
            repaint.push("sineLevel", "rampLevel");
        }
        for (const key of repaint) knobs.get(key).paint();
        if (repaint.includes(exactSelect.value)) updateExactValue();
        if (envelopeControls.has(id)) drawEnvelope();
        if (filterControls.has(id) && scopeActive && scopeMode.value === "filter") drawSignal();
        markEdited();
        send("controls", changes);
    }));

    const matrix = byId("matrix");
    function axis(label) {
        const span = document.createElement("span");
        span.className = "axis";
        span.textContent = label;
        span.setAttribute("aria-hidden", "true");
        return span;
    }

    function region() {
        return [selectedRow & ~3, selectedCol & ~3];
    }

    function selectCell(row, col, focus = false) {
        const oldRegion = region().join(",");
        selectedRow = row;
        selectedCol = col;
        if (oldRegion !== region().join(",")) drawMatrix();
        else updateMatrix();
        if (focus) cells.get(row * 16 + col)?.focus();
    }

    function updatePinRack() {
        pinsInUse = pins.reduce((used, pin) => used + Number(pin !== 0), 0);
        const remaining = Math.max(0, pinStock.length - pinsInUse);
        for (const [index, slot] of pinStock.entries())
            slot.dataset.used = String(index >= remaining);
        for (const button of pinButtons)
            button.setAttribute("aria-pressed", String(button.dataset.pinType === selectedPin));
        byId("pin-selection").textContent = remaining + "/" + pinStock.length + " livres";
        const overflow = pinsInUse - pinStock.length;
        pinStatus.hidden = overflow <= 0;
        pinStatus.textContent = overflow > 0 ? "Este patch usa " + overflow + " pino(s) além da bandeja." : "";
    }

    function setMatrixPin(index, row, col) {
        const current = pinColor(index);
        const nextColor = selectedPin === current ? "" : selectedPin;
        const next = pinTypes[nextColor]?.amount ?? 0;
        const previous = pins[index];
        if (nextColor === current) return true;
        if (!previous && next && pinsInUse >= pinStock.length) return false;
        pins[index] = next;
        if (nextColor === "red") pinColors.set(index, nextColor);
        else pinColors.delete(index);
        if (previous !== next) send("pin", row, col, next);
        markEdited();
        return true;
    }

    function updateMatrix() {
        const [firstRow, firstCol] = region();
        const selectedIndex = selectedRow * 16 + selectedCol;
        const destination = Array.from({ length: 16 }, (_, col) => {
            if (!ksMatrix.checked && ksRoutes[col]) return true;
            for (let row = 0; row < 16; row++) {
                if (pins[row * 16 + col]) return true;
            }
            return false;
        });
        for (const [col, label] of columnLabels) {
            label.classList.toggle("active", destination[col]);
            label.classList.toggle("guide-focus", col === selectedCol);
        }
        for (const [row, label] of rowLabels) {
            if (label.classList.contains("source-label") && row === 15)
                label.textContent = ksMatrix.checked ? "KS PITCH" : sources[row][1];
            label.classList.toggle("guide-focus", row === selectedRow);
        }
        [8, 9, 10, 13].forEach((col, i) => {
            const routed = destination[col];
            frequencyOutputs[i].dataset.modulated = String(routed);
            frequencyOutputs[i].title = routed ? "Recebe modulação" : "Sem rota de modulação";
        });
        for (const [index, cell] of cells) {
            const row = Math.floor(index / 16), col = index % 16;
            const pin = pins[index];
            const color = pinColor(index);
            cell.dataset.pinColor = color;
            const on = selectedIndex === index;
            const label = coordinate(row, col) + " · " + (row === 15 && ksMatrix.checked ? "KS PITCH" : sources[row][1]) + " → " + destinations[col][1];
            const state = supported(row, col)
                ? (pin ? pinTypes[color].label + " · " + (pin === 1 ? "100%" : "50%") : "sem pino") + (delayed(row, col) ? " · atraso de 1 amostra" : "")
                : "indisponível";
            cell.title = label + " · " + state;
            cell.tabIndex = on ? 0 : -1;
            cell.classList.toggle("selected", on);
            cell.classList.toggle("guide-line", row === selectedRow || col === selectedCol);
            cell.classList.toggle("destination", destination[col]);
            cell.classList.toggle("full", pin === 1);
            cell.classList.toggle("half", pin === 2);
            cell.classList.toggle("quadrant", !zoom && (row & ~3) === firstRow && (col & ~3) === firstCol);
            cell.setAttribute("aria-label", cell.title);
            cell.setAttribute("aria-pressed", String(pin !== 0));
            cell.setAttribute("aria-disabled", String(!supported(row, col)));
        }
        const first = coordinate(firstRow, firstCol);
        const last = coordinate(firstRow + 3, firstCol + 3);
        byId("region-status").textContent = first + " – " + last;
        regionButtons.forEach((button, i) => button.setAttribute(
            "aria-pressed", String(i === firstRow + firstCol / 4)
        ));
        byId("route-status").textContent = cells.get(selectedIndex)?.title ?? "";
        updatePinRack();
    }

    function drawMatrix() {
        const size = zoom ? 4 : 16;
        const [firstRow, firstCol] = zoom ? region() : [0, 0];
        matrix.replaceChildren();
        matrix.style.setProperty("--size", size);
        matrix.classList.toggle("zoom", zoom);
        cells.clear();
        columnLabels.length = rowLabels.length = 0;
        const place = (element, row, col) => {
            element.style.gridRow = row;
            element.style.gridColumn = col;
            matrix.append(element);
            return element;
        };
        for (const [start, end, name] of [[0, 8, "SIGNAL INPUTS"], [8, 16, "CONTROL INPUTS"]]) {
            const left = Math.max(start, firstCol), right = Math.min(end, firstCol + size);
            if (left >= right) continue;
            const label = axis(name);
            label.classList.add("destination-group");
            place(label, 1, (left - firstCol + 2) + " / span " + (right - left));
        }
        for (let col = firstCol; col < firstCol + size; col++) {
            const letter = axis(String.fromCharCode(65 + col));
            const label = axis(destinations[col][1]);
            label.classList.add("destination-label");
            if (col === 1) {
                label.classList.add("meter-destination");
                letter.classList.add("meter-destination");
            }
            columnLabels.push([col, letter], [col, label]);
            place(label, 2, col - firstCol + 2);
            place(letter, size + 3, col - firstCol + 2);
        }
        for (let row = firstRow; row < firstRow + size; row++) {
            const source = axis(sources[row][1]);
            source.classList.add("source-label");
            const number = axis(String(row + 1));
            rowLabels.push([row, source], [row, number]);
            place(source, row - firstRow + 3, 1);
            place(number, row - firstRow + 3, size + 2);
            for (let col = firstCol; col < firstCol + size; col++) {
                const cell = document.createElement("button");
                const index = row * 16 + col;
                cell.type = "button";
                cell.className = "cell";
                cell.classList.toggle("available", supported(row, col));
                if (col === 1) cell.classList.add("meter-jack");
                if (col % 4 === 3 || row === 6 || row === 8 || row === 13) cell.dataset.divider = "";
                cells.set(index, cell);
                place(cell, row - firstRow + 3, col - firstCol + 2);
                cell.addEventListener("click", () => {
                    const done = !supported(row, col) || setMatrixPin(index, row, col);
                    selectCell(row, col, true);
                    if (!done) {
                        pinStatus.hidden = false;
                        pinStatus.textContent = "A bandeja está vazia. Retire um pino para reutilizá-lo.";
                    }
                });
            }
        }
        updateMatrix();
    }

    for (const button of pinButtons) button.addEventListener("click", () => {
        selectedPin = button.dataset.pinType;
        updatePinRack();
    });
    for (const [title, labels] of [
        ["1–16 · Fontes", sources.map(([, name], i) => [i + 1, name])],
        ["A–H · Entradas de sinal", destinations.slice(0, 8).map(([, name], i) => [String.fromCharCode(65 + i), name])],
        ["I–P · Entradas de controle", destinations.slice(8).map(([, name], i) => [String.fromCharCode(73 + i), name])]
    ]) {
        const group = document.createElement("section");
        const heading = document.createElement("h3"), list = document.createElement("dl");
        heading.textContent = title;
        for (const [key, name] of labels) {
            const dt = document.createElement("dt"), dd = document.createElement("dd");
            dt.textContent = key;
            dd.textContent = name;
            list.append(dt, dd);
        }
        group.append(heading, list);
        byId("pin-reference-grid").append(group);
    }

    matrix.addEventListener("keydown", event => {
        const delta = { ArrowUp: [-1, 0], ArrowDown: [1, 0], ArrowLeft: [0, -1], ArrowRight: [0, 1] }[event.key];
        if (!delta) return;
        event.preventDefault();
        selectCell((selectedRow + delta[0] + 16) % 16, (selectedCol + delta[1] + 16) % 16, true);
    });

    for (let row = 0; row < 16; row += 4) {
        for (let col = 0; col < 16; col += 4) {
            const button = document.createElement("button");
            button.type = "button";
            button.textContent = coordinate(row, col);
            button.setAttribute("aria-label", "Região " + coordinate(row, col) + " a " + coordinate(row + 3, col + 3));
            button.addEventListener("click", () => {
                selectCell(row, col);
                setZoom(true);
            });
            regionButtons.push(button);
            byId("regions").append(button);
        }
    }

    function setZoom(value) {
        zoom = value;
        byId("matrix-region").hidden = !zoom;
        byId("overview").setAttribute("aria-pressed", String(!zoom));
        byId("edit").setAttribute("aria-pressed", String(zoom));
        drawMatrix();
    }

    function applyPatch(candidate, preset = "", custom = false, remote = false) {
        const patch = validatePatch(candidate);
        if (!patch) return false;
        if (!custom) resetCustomSelection(patch.name || "");
        for (const input of allControls) input.value = patch.controls[input.id];
        pins.fill(0);
        pinColors.clear();
        for (const [row, col, color] of patch.pinColors ?? []) pinColors.set(row * 16 + col, color);
        for (const [row, col, pin] of patch.pins) pins[row * 16 + col] = pin;
        ksRoutes.fill(0);
        for (const [col, amount] of patch.ksRoutes) ksRoutes[col] = amount;
        ksMatrix.checked = patch.ksMatrix;
        usbRoutes.fill(0);
        for (const [channel, col, amount] of patch.usbRoutes ?? []) usbRoutes[channel * 16 + col] = amount;
        for (const [at, select] of usbRouteControls) select.value = usbRoutes[at];
        if (patch.midiCC) {
            midiCC = patch.midiCC;
            updateMidiCC();
        }
        if (patch.stepGrid) {
            stepGrid = patch.stepGrid;
            activeStep = stepPage = 0;
            renderSteps();
        }
        if (patch.performance) {
            const perf = patch.performance;
            arpEnabled = perf.enabled;
            for (const id of ["bpm", "gate", "order", "division", "chance"]) byId("arp-" + id).value = perf[id];
            byId("arp-hold").checked = perf.hold;
            byId("arp-free").checked = perf.freeRun;
            byId("scale-root").value = perf.root;
            byId("scale-mode").value = perf.scale;
            changeArp();
        }
        updateKsRoutes();
        byId("preset").value = preset;
        highlightInstantPatch(preset);
        const keepSequence = remote && JSON.stringify(portableSequence) === JSON.stringify(patch.sequence);
        if (!keepSequence) {
            portableSequence = patch.sequence;
            ksFromGrid = false;
            playingStepGrid = false;
            ksRevision++;
            ksPhraseEditor.show(portableSequence);
            updateKsStatus(phraseStatus());
        }
        patchName = patch.name;
        byId("studio-patch-name").textContent = byId("matrix-patch-name").textContent =
            byId("preset-drawer-current").textContent = patchName || "Patch do SMK";
        if (!remote) {
            matrixPreset.value = Object.hasOwn(FactoryCatalog, preset) ? "factory:" + preset : "";
            liveEdit.updateActions?.();
        }
        updateTryAudioRoutes();
        presetGuide.textContent = presets[preset]?.play || (patch.sequence ? "Frase carregada no KS. Pressione Play." : "");
        presetGuide.hidden = !presetGuide.textContent;
        const settings = patch.ks ?? { realtimeSpread: 100, sequenceSpread: 100, speed: 100,
                                       transpose: 0, limitSeconds: 12, loop: true };
        for (const [id, key] of ksSettings) byId("ks-" + id).value = settings[key];
        byId("ks-loop").checked = settings.loop;
        ksTrigger.value = patch.sequenceTrigger ? "sequence" :
            !patch.ksAutoTrigger && patch.sequence && candidate.sequenceTrigger === false ? "live" : "auto";
        knobs.forEach(knob => knob.paint());
        updateExactValue();
        drawEnvelope();
        updateMatrix();
        if (scopeActive && scopeMode.value === "filter") drawSignal();
        send("patch", patch, keepSequence, ksRevision);
        updateKsControls();
        paintTransposePads();
        return true;
    }

    function loadPreset(key) {
        if (!presets[key] || !applyPatch({ version: 1, ...presets[key] }, key)) return false;
        byId("patch-status").textContent = presets[key].name + " carregado.";
        return true;
    }

    byId("overview").addEventListener("click", () => setZoom(false));
    byId("edit").addEventListener("click", () => setZoom(true));
    byId("reset").addEventListener("click", () => loadPreset("initial"));
    byId("preset").addEventListener("change", event => loadPreset(event.target.value));
    byId("clear").addEventListener("click", () => {
        for (let i = 0; i < pins.length; i++) {
            if (pins[i]) send("pin", i >> 4, i & 15, 0);
        }
        pins.fill(0);
        pinColors.clear();
        updateMatrix();
        markEdited();
        byId("patch-status").textContent = "Todos os pinos retirados.";
    });
    byId("export").addEventListener("click", () => {
        const name = byId("user-name").value.trim() || patchName;
        const payload = name ? { ...patchSnapshot(), name } : patchSnapshot();
        const url = URL.createObjectURL(new Blob([JSON.stringify(payload, null, 2)], { type: "application/json" }));
        const link = document.createElement("a");
        link.href = url;
        link.download = "zivisynthi-patch.json";
        link.click();
        setTimeout(() => URL.revokeObjectURL(url), 1000);
    });
    byId("import").addEventListener("click", () => byId("file").click());
    byId("file").addEventListener("change", async event => {
        const file = event.target.files[0];
        event.target.value = "";
        if (!file) return;
        try {
            if (file.size > 64 * 1024 || !applyPatch(JSON.parse(await file.text()))) {
                throw new Error("Formato incompatível");
            }
            byId("patch-status").textContent = "Patch importado.";
        } catch {
            byId("patch-status").textContent = "Patch inválido. As conexões atuais foram mantidas.";
        }
    });

    function syncAudio() {
        processor?.port.postMessage({ type: "aks-profile", values: [aksProfile.value === "1"] });
        processor?.port.postMessage({ type: "patch", values: [patchSnapshot(), false, ksRevision] });
        updateKsControls();
        for (const axis of stickAxes)
            processor?.port.postMessage({ type: axis.type, values: [axis.value] });
        processor?.port.postMessage({ type: "arp-settings", values: [arpSettings()] });
        processor?.port.postMessage({ type: "scale-settings", values: scaleSettings() });
        for (const [note, velocity] of inputNotes.values()) send("note", note, velocity);
        if (muted) send("panic");
    }

    async function startAudio() {
        if (audioStart) return audioStart;
        audioStart = (async () => {
            try {
                const Audio = window.AudioContext ?? window.webkitAudioContext;
                if (!Audio) throw new Error("Web Audio indisponível neste navegador");
                audioContext = new Audio({ latencyHint: "interactive" });
                if (!audioContext.audioWorklet) throw new Error("Áudio indisponível. Abra no Chrome ou Edge por HTTPS ou localhost.");
                await audioContext.resume();
                if (location.protocol === "file:") {
                    // Chromium rejects blob:null modules when the Studio is opened from disk.
                    const module = "data:text/javascript;charset=utf-8," + encodeURIComponent(ZIVI_WORKLET);
                    await audioContext.audioWorklet.addModule(module);
                } else {
                    const worklet = URL.createObjectURL(new Blob([ZIVI_WORKLET], { type: "text/javascript" }));
                    try {
                        await audioContext.audioWorklet.addModule(worklet);
                    } finally {
                        URL.revokeObjectURL(worklet);
                    }
                }
                processor = new AudioWorkletNode(audioContext, "zivisynthi", {
                    numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2], channelCount: 2,
                    channelCountMode: "explicit", channelInterpretation: "speakers"
                });
                processor.port.onmessage = ({ data }) => {
                    if (data.type === "ks-capture") {
                        if (data.revision !== ksRevision || hardwarePerformance?.editing) return;
                        if (data.sequence) {
                            portableSequence = data.sequence;
                            ksFromGrid = false;
                            ksPhraseEditor.show(portableSequence);
                            markEdited();
                        } else {
                            if (portableSequence) replaceKsPhrase(portableSequence);
                            if (data.eventCount) {
                                byId("patch-status").textContent = "A gravação é curta demais para exportação em milissegundos.";
                            }
                        }
                        return;
                    }
                    byId("meter").value = data.output;
                    byId("matrix-meter").value = data.matrix;
                    if (!hardwarePerformance?.editing) {
                        updateKsStatus(data.ks);
                        paintArpNote(data.arpNote);
                    }
                    telemetry = data;
                    updatePanelMeter();
                    const db = data.output > 0 ? 20 * Math.log10(data.output) : -Infinity;
                    byId("meter-db").textContent = Number.isFinite(db) ? decimal(db) + " dBFS" : "−∞ dBFS";
                    data.hz.forEach((hz, index) => { frequencyOutputs[index].textContent = formatHz(hz); });
                    if (data.envelope !== phase) {
                        phase = data.envelope;
                        envelopeRack.parentElement.dataset.phase = phase === 0 ? "off" : "active";
                        byId("envelope-phase").textContent = ["Off", "Attack", "On", "Decay"][phase];
                        drawEnvelope();
                    }
                    byId("envelope-meter").value = data.contour;
                    if (scopeActive) drawSignal();
                };
                splitter = audioContext.createChannelSplitter(2);
                analyser = audioContext.createAnalyser();
                analyser.fftSize = waveBuffer.length;
                analyser.smoothingTimeConstant = 0.72;
                processor.connect(splitter);
                processor.connect(audioContext.destination);
                connectAnalyser();
                syncAudio();
                audioStatus.textContent = "Áudio ativo" + " · " + audioContext.sampleRate.toLocaleString(locale) + " Hz";
                audioButton.textContent = "Áudio ativo";
                audioButton.disabled = true;
            } catch (error) {
                await audioContext?.close().catch(() => {});
                audioContext = undefined;
                audioStart = undefined;
                processor = splitter = analyser = telemetry = undefined;
                audioStatus.textContent = error.message;
            }
        })();
        return audioStart;
    }
    audioButton.addEventListener("click", startAudio);
    const tryAudioFile = byId("try-audio-file"), tryAudioStatus = byId("try-audio-status");
    const tryAudioPlay = byId("try-audio-play"), tryAudioStop = byId("try-audio-stop");
    let tryFileBuffer, tryFileSource, tryFileRevision = 0;
    function stopTryFile() {
        const source = tryFileSource;
        tryFileSource = undefined;
        if (source) {
            source.disconnect();
            source.stop();
        }
        tryAudioStop.disabled = true;
    }
    function updateTryAudioRoutes() {
        const active = usbRoutes.some(Boolean);
        byId("try-audio-input").hidden = !active;
        if (!active) stopTryFile();
    }
    tryAudioFile.addEventListener("change", async () => {
        const file = tryAudioFile.files[0];
        if (!file) return;
        const revision = ++tryFileRevision;
        stopTryFile();
        tryFileBuffer = undefined;
        tryAudioPlay.disabled = true;
        tryAudioStatus.textContent = "Carregando áudio…";
        try {
            if (file.size > 32 * 1024 * 1024) throw new Error("Escolha um arquivo de áudio de até 32 MB.");
            if (!await enterTry(tryAudioStatus)) return;
            const decoded = await audioContext.decodeAudioData(await file.arrayBuffer());
            if (revision !== tryFileRevision) return;
            tryFileBuffer = decoded;
            tryAudioPlay.disabled = false;
            tryAudioStatus.textContent = file.name;
        } catch (error) {
            if (revision === tryFileRevision)
                tryAudioStatus.textContent = error.message === "Escolha um arquivo de áudio de até 32 MB."
                    ? error.message : "O navegador não conseguiu decodificar o arquivo de áudio.";
        }
    });
    tryAudioPlay.addEventListener("click", async () => {
        if (!tryFileBuffer || !await enterTry(tryAudioStatus) || byId("try-audio-input").hidden) return;
        stopTryFile();
        const source = audioContext.createBufferSource();
        source.buffer = tryFileBuffer;
        source.connect(processor);
        source.onended = () => { if (tryFileSource === source) stopTryFile(); };
        tryFileSource = source;
        tryAudioStop.disabled = false;
        source.start();
        tryAudioStatus.textContent = "Áudio encaminhado pela matriz USB.";
    });
    tryAudioStop.addEventListener("click", stopTryFile);
    const instantPanel = byId("instant-patchers");
    const instantButtons = [...instantPanel.querySelectorAll("[data-instant-patch]")];
    const tryButton = byId("try");
    let choosingPatch = false;

    function showInstantPatches() {
        instantPanel.hidden = false;
        tryButton.setAttribute("aria-expanded", "true");
    }

    function highlightInstantPatch(key) {
        for (const button of instantButtons) {
            button.setAttribute("aria-pressed", String(button.dataset.instantPatch === key));
        }
    }

    async function enterTry(status) {
        if (!liveEdit.pauseEditing()) {
            status.textContent = "Aguarde a transferência USB para abrir o modo Try.";
            return false;
        }
        if (hardwarePerformance?.editing) reportHardware({ ...hardwarePerformance, editing: false });
        status.textContent = "";
        if (pendingTryGrid) syncSavedStepGrid(pendingTryGrid, byId("step-status"));
        await startAudio();
        if (!processor) {
            status.textContent = audioStatus.textContent;
            return false;
        }
        if (muted) setMuted(false);
        return true;
    }

    function followTryLink() {
        if (location.hash === "#try" || location.hash === "#instant-patchers") showInstantPatches();
    }
    followTryLink();
    window.addEventListener("hashchange", followTryLink);
    tryButton.addEventListener("click", async () => {
        showInstantPatches();
        if (await enterTry(byId("try-status"))) {
            byId("instant-patchers-title").focus({ preventScroll: true });
            instantPanel.scrollIntoView({ block: "nearest" });
        }
    });
    for (const button of instantButtons) button.addEventListener("click", async () => {
        if (choosingPatch) return;
        const status = byId("instant-status");
        choosingPatch = true;
        try {
            if (!await enterTry(status)) return;
            const key = button.dataset.instantPatch;
            if (!loadPreset(key)) return;
            if (portableSequence) {
                if (byId("seq-ks").hidden) document.querySelector('[data-seq="ks"]').click();
                await runKs("play");
                status.textContent = presets[key].name + " · frase KS em reprodução.";
            } else status.textContent = presets[key].name + " · toque no teclado.";
        } finally { choosingPatch = false; }
    });
    async function loadLocalMatrixPreset(key, status) {
        if (!Object.hasOwn(FactoryCatalog, key) || !await enterTry(status)) return;
        const phrase = ksFromGrid && !presets[key].sequence ? portableSequence : undefined;
        if (!loadPreset(key)) return;
        if (phrase) {
            replaceKsPhrase(phrase);
            ksFromGrid = true;
        }
        status.textContent = presets[key].name + " · preset de fábrica carregado no navegador.";
        if (presets[key].sequence) await runKs("play");
    }

    const attackButton = byId("manual-attack");
    let attackHeld = false, attackKeepalive = 0, attackPulse = 0;
    function attackDown() {
        if (attackHeld) return;
        attackHeld = true;
        if (muted) setMuted(false);
        attackButton.setAttribute("aria-pressed", "true");
        send("manual-attack", true);
        attackKeepalive = window.setInterval(() => liveEdit("manual-attack", [true]), 230);
        startAudio().then(() => {
            if (attackHeld) processor?.port.postMessage({ type: "manual-attack", values: [true] });
        });
    }
    function attackUp() {
        if (!attackHeld) return;
        attackHeld = false;
        window.clearInterval(attackKeepalive);
        window.clearTimeout(attackPulse);
        attackButton.setAttribute("aria-pressed", "false");
        send("manual-attack", false);
    }
    attackButton.addEventListener("pointerdown", event => {
        if (event.button !== 0) return;
        attackButton.setPointerCapture(event.pointerId);
        attackDown();
    });
    for (const type of ["pointerup", "pointercancel", "lostpointercapture", "blur"])
        attackButton.addEventListener(type, attackUp);
    for (const type of ["keydown", "keyup"]) attackButton.addEventListener(type, event => {
        if (event.key !== " " && event.key !== "Enter") return;
        event.preventDefault();
        if (type === "keydown") attackDown();
        else attackUp();
    });
    attackButton.addEventListener("click", event => {
        if (event.detail !== 0 || attackHeld) return;
        attackDown();
        attackPulse = window.setTimeout(attackUp, 140);
    });
    window.addEventListener("blur", attackUp);
    const stickPad = byId("stick-pad"), stickRoute = byId("stick-route");
    const stickAxes = ["x", "y"].map(axis => ({
        type: "stick-" + axis, range: byId("stick-range-" + axis), position: 0, value: 0
    }));
    const [stickX, stickY] = stickAxes;
    let remoteStickRanges = false;
    function paintStick() {
        stickPad.style.setProperty("--stick-x", (50 + 43 * stickX.value) + "%");
        stickPad.style.setProperty("--stick-y", (50 - 43 * (stickY.range.disabled ? 0 : stickY.value)) + "%");
    }
    for (const [index, axis] of stickAxes.entries()) {
        const { range } = axis;
        axis.paintRange = ZiviVisuals.knob(range, byId(range.id + "-value"), percent).paint;
        axis.paintRange();
        range.addEventListener("input", () => {
            axis.paintRange();
            axis.value = axis.position * range.valueAsNumber;
            paintStick();
            if (range.disabled) return;
            if (remoteStickRanges && liveEdit("panel-config", [16 + index, Math.round(range.valueAsNumber * 4096)]))
                processor?.port.postMessage({ type: axis.type, values: [axis.value] });
            else send(axis.type, axis.value, axis.position);
        });
    }
    function positionStick(x, y) {
        for (const [axis, value] of [[stickX, x], [stickY, y]]) {
            if (axis.range.disabled || !Number.isFinite(value)) continue;
            const position = Math.max(-1, Math.min(1, value));
            const ranged = position * axis.range.valueAsNumber;
            if (axis.position === position && axis.value === ranged) continue;
            axis.position = position;
            axis.value = ranged;
            send(axis.type, axis.value, axis.position);
        }
        paintStick();
    }
    function moveStick(event) {
        const box = stickPad.getBoundingClientRect();
        positionStick(2 * (event.clientX - box.left) / box.width - 1,
            1 - 2 * (event.clientY - box.top) / box.height);
    }
    stickPad.addEventListener("pointerdown", event => {
        if (event.button !== 0) return;
        stickPad.focus();
        stickPad.setPointerCapture(event.pointerId);
        moveStick(event);
    });
    stickPad.addEventListener("pointermove", event => {
        if (stickPad.hasPointerCapture(event.pointerId)) moveStick(event);
    });
    stickPad.addEventListener("keydown", event => {
        const step = event.shiftKey ? 0.01 : 0.1;
        switch (event.key) {
        case "ArrowLeft": positionStick(stickX.position - step); break;
        case "ArrowRight": positionStick(stickX.position + step); break;
        case "ArrowDown": positionStick(undefined, stickY.position - step); break;
        case "ArrowUp": positionStick(undefined, stickY.position + step); break;
        case "Home": positionStick(0, 0); break;
        default: return;
        }
        event.preventDefault();
    });
    byId("stick-center").addEventListener("click", () => positionStick(0, 0));
    paintStick();

    const ksRouteControls = destinations.map(([, name], col) => {
        const label = document.createElement("label"), select = document.createElement("select");
        label.textContent = String.fromCharCode(65 + col) + " · " + name;
        for (const [title, amount] of [["Desligado", 0], ["50%", 2], ["100%", 1]])
            select.add(new Option(title, amount));
        select.addEventListener("change", () => {
            ksRoutes[col] = Number(select.value);
            send("ks-route", col, ksRoutes[col]);
            updateMatrix();
            markEdited();
        });
        label.append(select);
        byId("ks-routes").append(label);
        return select;
    });

    function updateKsRoutes() {
        for (const [col, select] of ksRouteControls.entries()) {
            select.value = ksRoutes[col];
            select.disabled = ksMatrix.checked;
        }
        stickY.range.disabled = ksMatrix.checked;
        stickRoute.textContent = ksMatrix.checked ? "X → 15 · KS → 16" : "X → 15 · Y → 16";
        paintStick();
    }

    const stepButtons = [], stepPageButtons = [];
    let stepPage = 0;
    const noteName = note => names[note % 12] + (Math.floor(note / 12) - 1);
    function renderSteps() {
        const [note, velocity] = stepGrid.steps[activeStep];
        byId("step-length").value = stepGrid.length;
        byId("step-bpm").value = stepGrid.bpm;
        byId("step-division").value = stepGrid.division;
        byId("step-note").value = note;
        byId("step-velocity").value = velocity;
        byId("step-position").textContent = activeStep + 1;
        const first = byId("step-first"), last = byId("step-last");
        for (const field of [first, last]) {
            field.max = stepGrid.length;
            if (field.valueAsNumber > stepGrid.length) field.value = stepGrid.length;
        }
        const pageCount = Math.ceil(stepGrid.length / 16);
        stepPage = Math.min(stepPage, pageCount - 1);
        for (const [page, button] of stepPageButtons.entries()) {
            button.disabled = page >= pageCount;
            button.setAttribute("aria-pressed", String(page === stepPage));
        }
        for (const [at, button] of stepButtons.entries()) {
            const [number, vel, legato] = stepGrid.steps[at];
            button.hidden = at >= stepGrid.length || (at >> 4) !== stepPage;
            button.setAttribute("aria-pressed", String(at === activeStep));
            button.classList.toggle("sounding", vel > 0);
            button.classList.toggle("in-range", at + 1 >= first.valueAsNumber && at + 1 <= last.valueAsNumber);
            button.children[1].textContent = vel ? noteName(number) : "—";
            button.style.setProperty("--velocity", (vel / 127 * 100) + "%");
            button.setAttribute("aria-label", "Passo " + (at + 1) + (vel ?
                " · " + noteName(number) + " · velocidade " + vel + (legato ? " · ligado" : "") : " · pausa"));
        }
    }
    for (let page = 0; page < 4; ++page) {
        const button = document.createElement("button");
        button.type = "button";
        button.textContent = (page * 16 + 1) + "–" + ((page + 1) * 16);
        button.addEventListener("click", () => {
            stepPage = page;
            activeStep = page * 16;
            renderSteps();
        });
        byId("step-pages").append(button);
        stepPageButtons.push(button);
    }
    for (let at = 0; at < 64; ++at) {
        const button = document.createElement("button");
        const index = document.createElement("small"), note = document.createElement("strong");
        button.type = "button";
        index.textContent = String(at + 1).padStart(2, "0");
        button.append(index, note);
        button.addEventListener("click", event => {
            const anchor = event.shiftKey ? activeStep : at;
            activeStep = at;
            byId("step-first").value = Math.min(at, anchor) + 1;
            byId("step-last").value = Math.max(at, anchor) + 1;
            renderSteps();
        });
        byId("step-grid").append(button);
        stepButtons.push(button);
    }
    function loadStepGrid(candidate, feedback) {
        const grid = ZiviDSP.validateStepGrid(candidate);
        if (!grid) return false;
        stepGrid = grid;
        activeStep = Math.min(activeStep, grid.length - 1);
        stepPage = activeStep >> 4;
        if (feedback) {
            byId("step-live").checked = false;
            byId("step-status").textContent = feedback + (liveEdit.isEditing() ? " Use Enviar SEQ para atualizar o teclado." : "");
            markEdited();
        } else {
            byId("step-status").textContent = "SEQ lido do SMK.";
        }
        renderSteps();
        return true;
    }
    function copyStepGridToKs(grid, status, append = false) {
        const phrase = ZiviSequences.toKs(grid);
        const message = phrase ? "SEQ carregado no KS." :
            grid.steps.slice(0, grid.length).some(step => step[1])
                ? "O SEQ ultrapassa o limite de 30 s do KS."
                : "O SEQ não contém notas ativas.";
        if (phrase) {
            if (ksTrigger.value === "live") ksTrigger.value = "auto";
            replaceKsPhrase(phrase);
            ksFromGrid = true;
            markEdited();
        }
        status.textContent = append && status.textContent ? status.textContent + " " + message : message;
        return Boolean(phrase);
    }
    function syncSavedStepGrid(grid, status) {
        if (liveEdit.isEditing()) {
            pendingTryGrid = grid;
            return;
        }
        pendingTryGrid = undefined;
        copyStepGridToKs(grid, status, true);
    }
    ZiviSequences.setup(byId, () => stepGrid, loadStepGrid, () => activeStep, renderSteps, syncSavedStepGrid);
    const intField = (id, low, high, fallback) => {
        const value = byId(id).valueAsNumber;
        return Number.isFinite(value) ? Math.max(low, Math.min(high, Math.round(value))) : fallback;
    };
    const midiControl = byId("midi-cc-control"), midiValue = byId("midi-cc-value");
    const aksProfile = byId("aks-profile");
    aksProfile.value = "1";
    aksProfile.addEventListener("change", () => {
        liveEdit("panel-config", [20, Number(aksProfile.value)]);
        processor?.port.postMessage({ type: "aks-profile", values: [aksProfile.value === "1"] });
        drawEnvelope();
    });
    const midiBanks = [byId("midi-bank-a"), byId("midi-bank-b")];
    const knobFormat = byId("midi-knob-format");
    for (const [bank, select] of midiBanks.entries()) {
        PanelCatalog.forEach(({ title }, index) => select.add(new Option(title, String(index))));
        select.value = bank === 0 ? "0" : "2";
    }
    knobFormat.addEventListener("change", () => send("panel-config", 12, Number(knobFormat.value === "relative")));
    const panelConfig = byId("smk-panel-config");
    const switches = ["Desligado", "Ligado"];
    const panelFields = [
        ["Master (%)", 0, 100], ["Sintetizador (%)", 0, 100], ["Retorno USB (%)", 0, 100],
        ["Áudio USB", ["Monitor", "Matriz"]], ["Canal de entrada", 0, 16], ["Canal de saída", 1, 16],
        ["Destino MIDI", ["Todos", "TRS", "USB", "BLE"]], ["Clock", ["Interno", "Externo"]],
        ["Enviar clock", switches], ["Referência KS (BPM)", 30, 300], ["Enviar notas KS", switches],
        ["USB Thru", switches], ["CC dos knobs", ["Absoluto", "Relativo"]], ["LED dos pads", ["Fixo", "Animado", "Desligado"]]
    ].map(([name, low, high], index) => {
        const label = document.createElement("label");
        label.textContent = name;
        const field = document.createElement(Array.isArray(low) ? "select" : "input");
        if (Array.isArray(low)) low.forEach((option, i) => field.add(new Option(option, String(i))));
        else {
            field.type = "number";
            field.min = low;
            field.max = high;
            field.step = index < 3 ? 0.1 : 1;
        }
        field.disabled = true;
        field.addEventListener("change", () => {
            const value = Number(field.value);
            if (field.value !== "" && field.checkValidity()) {
                if (index === 12) knobFormat.value = value ? "relative" : "absolute";
                send("panel-config", index, index < 3 ? Math.round(value * 4096 / 100) : value);
            }
        });
        label.append(field);
        byId("smk-panel-fields").append(label);
        return field;
    });
    function reportPanel(values, editing) {
        panelConfig.hidden = !values;
        remoteStickRanges = values?.length >= 20;
        aksProfile.disabled = editing && values?.length < 21;
        if (!values) return;
        if (editing) {
            const profile = String(values[20] ?? 0);
            if (aksProfile.value !== profile) {
                aksProfile.value = profile;
                processor?.port.postMessage({ type: "aks-profile", values: [profile === "1"] });
                drawEnvelope();
            }
        }
        for (const [index, field] of panelFields.entries()) {
            field.disabled = !editing;
            const value = index < 3 ? (100 * values[index] / 4096).toFixed(1) : String(values[index]);
            if (field.value !== value) field.value = value;
        }
        const format = values[12] ? "relative" : "absolute";
        if (knobFormat.value !== format) knobFormat.value = format;
        if (editing) {
            let moved = false;
            for (const [index, axis] of stickAxes.entries()) {
                const centered = values[14 + index] - 8192;
                const value = centered / (centered < 0 ? 8192 : 8191);
                if (remoteStickRanges) {
                    const range = values[16 + index] / 4096;
                    if (Math.abs(axis.range.valueAsNumber - range) > 0.5 / 4096) {
                        axis.range.value = range;
                        axis.paintRange();
                    }
                    const raw = values[18 + index] - 8192;
                    axis.position = raw / (raw < 0 ? 8192 : 8191);
                } else if (Math.abs(value - axis.value) >= 1 / 8191) {
                    const range = axis.range.valueAsNumber;
                    axis.position = range ? Math.max(-1, Math.min(1, value / range)) : 0;
                }
                if (Math.abs(value - axis.value) >= 1 / 8191) {
                    axis.value = value;
                    moved = true;
                    processor?.port.postMessage({ type: axis.type, values: [value] });
                }
            }
            if (moved) paintStick();
        }
    }
    function updateMidiCC() {
        midiValue.value = midiCC[Number(midiControl.value)];
    }
    for (let i = 0; i < 24; i++) {
        const knob = i < 16, bank = knob ? i < 8 : i < 20;
        midiControl.add(new Option((bank ? "Banco A · " : "Banco B · ") +
            (knob ? "K" + (i % 8 + 1) : "F" + (i % 4 + 1)), i));
    }
    midiControl.addEventListener("change", updateMidiCC);
    midiValue.addEventListener("change", () => {
        const index = Number(midiControl.value);
        midiCC[index] = intField("midi-cc-value", 0, 127, midiCC[index]);
        updateMidiCC();
        send("midi-cc", index, midiCC[index]);
    });
    for (const id of ["step-length", "step-bpm", "step-division"]) byId(id).addEventListener("change", () => {
        stepGrid.length = intField("step-length", 1, 64, stepGrid.length);
        stepGrid.division = Number(byId("step-division").value);
        stepGrid.bpm = intField("step-bpm", stepGrid.division === 4 ? 40 : 30, 300, stepGrid.bpm);
        activeStep = Math.min(activeStep, stepGrid.length - 1);
        stepPage = activeStep >> 4;
        if (byId("step-live").checked) send("step-grid", stepGrid.length, stepGrid.bpm, stepGrid.division);
        renderSteps();
        markEdited();
    });
    function changeStep(toggle) {
        const step = stepGrid.steps[activeStep];
        step[0] = intField("step-note", 0, 127, step[0]);
        step[1] = toggle ? (step[1] ? 0 : 100) : intField("step-velocity", 0, 127, step[1]);
        if (byId("step-live").checked) send("step-cell", activeStep, step[0], step[1]);
        renderSteps();
        markEdited();
    }
    byId("step-note").addEventListener("change", () => changeStep(false));
    byId("step-velocity").addEventListener("change", () => changeStep(false));
    byId("step-toggle").addEventListener("click", () => changeStep(true));
    for (const play of [false, true]) byId(play ? "step-play" : "step-stop").addEventListener("click", async () => {
        const status = byId("step-status");
        if (liveEdit.isEditing()) {
            liveEdit("step-play", [play]);
            status.textContent = "";
            return;
        }
        if (!play) {
            await runKs("stop");
            return;
        }
        if (!copyStepGridToKs(stepGrid, status)) return;
        playingStepGrid = true;
        await runKs("play");
    });
    byId("ks-use-step").addEventListener("click", () => {
        const status = byId("ks-step-status");
        if (liveEdit.isEditing()) status.textContent = "Abra o modo Try para carregar o SEQ no KS do navegador.";
        else copyStepGridToKs(stepGrid, status);
    });
    byId("step-record").disabled = true;
    byId("step-record").addEventListener("click", () => {
        byId("step-status").textContent = liveEdit("step-record", []) ?
            "Para recuperar a tomada, pare e clique em Ler SEQ do SMK." : "Vincule o SMK e aplique ou carregue o patch.";
    });
    renderSteps();

    for (let channel = 0; channel < 2; ++channel) {
        const group = document.createElement("fieldset");
        const legend = document.createElement("legend");
        legend.textContent = channel ? "USB R" : "USB L";
        group.append(legend);
        for (let col = 0; col < 16; ++col) {
            if (!supported(7, col)) continue;
            const at = channel * 16 + col;
            const label = document.createElement("label"), select = document.createElement("select");
            label.textContent = destinations[col][1];
            for (const [name, value] of [["Sem pino", 0], ["100%", 1], ["50%", 2]]) select.add(new Option(name, value));
            select.addEventListener("change", () => {
                usbRoutes[at] = Number(select.value);
                send("usb-route", channel, col, usbRoutes[at]);
                updateTryAudioRoutes();
                markEdited();
            });
            label.append(select);
            group.append(label);
            usbRouteControls.set(at, select);
        }
        byId("usb-routes").append(group);
    }

    byId("usb-migrate").addEventListener("click", () => {
        for (let ch = 0; ch < 2; ++ch) for (let col = 0; col < 16; ++col) {
            const at = (7 + ch) * 16 + col;
            if (!pins[at]) continue;
            usbRoutes[ch * 16 + col] = pins[at];
            usbRouteControls.get(ch * 16 + col).value = pins[at];
            pins[at] = 0;
            pinColors.delete(at);
            send("usb-route", ch, col, usbRoutes[ch * 16 + col]);
            send("pin", 7 + ch, col, 0);
        }
        updateTryAudioRoutes();
        updateMatrix();
        markEdited();
    });

    ksMatrix.addEventListener("change", () => {
        updateKsRoutes();
        updateMatrix();
        send("ks-matrix", ksMatrix.checked);
        markEdited();
    });

    const ksSettings = [
        ["realtime", "realtimeSpread", "%"], ["spread", "sequenceSpread", "%"],
        ["length", "limitSeconds", " s"], ["speed", "speed", "%"],
        ["transpose", "transpose", " st"]
    ];

    const ksPaint = new Map(ksSettings.map(([id, , unit]) => {
        const input = byId("ks-" + id);
        const label = input.parentElement;
        const caption = document.createElement("span");
        caption.textContent = label.firstChild.textContent.trim();
        label.firstChild.replaceWith(caption);
        label.className = "control";
        const value = byId("ks-" + id + "-value");
        label.append(value);
        return [id, ZiviVisuals.knob(input, value, number => number + unit).paint];
    }));

    function readKsSettings() {
        return {
            loop: byId("ks-loop").checked,
            sequenceTrigger: ksTrigger.value === "sequence",
            ...Object.fromEntries(ksSettings.map(([id, key]) => [key, byId("ks-" + id).valueAsNumber]))
        };
    }

    function updateKsControls() {
        for (const paint of ksPaint.values()) paint();
        send("ks-controls", readKsSettings());
    }

    function renderKsTimeline(position = 0) {
        const canvas = byId("ks-timeline"), ctx = canvas.getContext("2d");
        if (!ctx) return;
        const { width, height } = canvas;
        ctx.clearRect(0, 0, width, height);
        ctx.fillStyle = "#374655";
        for (let x = 0; x <= width; x += width / 8) ctx.fillRect(x, 0, 1, height);
        const seq = portableSequence;
        if (!seq?.events.length) return;
        const notes = seq.events.filter(event => event[2]).map(event => event[1]);
        const base = Math.min(...notes), span = Math.max(6, Math.max(...notes) - base + 1);
        ctx.fillStyle = "#7fbae9";
        seq.events.forEach(([time, note, velocity], i) => {
            if (!velocity) return;
            const x = width * time / seq.lengthMs;
            const end = width * (seq.events[i + 1]?.[0] ?? seq.lengthMs) / seq.lengthMs;
            const y = height - 6 - (height - 12) * (note - base) / span;
            ctx.fillRect(x, y, Math.max(2, end - x), 4);
        });
        ctx.fillStyle = "#ffc870";
        ctx.fillRect(Math.min(width - 2, position * 1000 * width / seq.lengthMs), 0, 2, height);
    }

    function arpSettings() {
        return { enabled: arpEnabled, bpm: byId("arp-bpm").valueAsNumber,
                 gate: byId("arp-gate").valueAsNumber, order: Number(byId("arp-order").value),
                 division: Number(byId("arp-division").value),
                 hold: byId("arp-hold").checked, freeRun: byId("arp-free").checked,
                 chance: byId("arp-chance").valueAsNumber };
    }
    function paintArpNote(note) {
        const label = note == null ? "Segure as notas" :
            chromaticNames[note % 12] + (Math.floor(note / 12) - 1);
        const node = byId("arp-note");
        if (node.textContent !== label) node.textContent = label;
    }
    function changeArp() {
        const bpm = byId("arp-bpm");
        bpm.value = Math.max(30, Math.min(300, Math.round(bpm.valueAsNumber || 120)));
        byId("arp-toggle").setAttribute("aria-pressed", arpEnabled);
        byId("arp-toggle").textContent = arpEnabled ? "Desativar ARP" : "Ativar ARP";
        byId("arp-gate-value").value = byId("arp-gate").value + "%";
        byId("arp-chance-value").value = byId("arp-chance").value + "%";
        send("arp-settings", arpSettings());
    }
    byId("arp-toggle").addEventListener("click", () => {
        arpEnabled = !arpEnabled;
        changeArp();
    });
    for (const id of ["arp-bpm", "arp-order", "arp-gate", "arp-division", "arp-chance", "arp-hold", "arp-free"]) byId(id).addEventListener("change", changeArp);
    for (const [i, note] of chromaticNames.entries()) byId("scale-root").add(new Option(note, String(i)));
    const scaleSelect = byId("scale-mode"), scaleGroups = new Map();
    ScaleCatalog.forEach(({name, group}, i) => {
        if (!scaleGroups.has(group)) {
            const options = document.createElement("optgroup");
            options.label = group;
            scaleGroups.set(group, options);
            scaleSelect.append(options);
        }
        scaleGroups.get(group).append(new Option(name, String(i)));
    });
    function limitPerformanceOptions(count, divisions, advanced = true) {
        const random = byId("arp-order").querySelector('option[value="3"]');
        random.disabled = !advanced;
        if (!advanced) {
            if (byId("arp-order").value === "3") byId("arp-order").value = "0";
            byId("arp-hold").checked = byId("arp-free").checked = false;
            byId("arp-chance").value = 100;
            byId("arp-chance-value").value = "100%";
        }
        for (const id of ["arp-hold", "arp-free", "arp-chance"]) byId(id).disabled = !advanced;
        byId("arp-division").disabled = !divisions;
        if (!divisions) byId("arp-division").value = "2";
        for (const option of scaleSelect.options) option.disabled = Number(option.value) >= count;
        if (Number(scaleSelect.value) >= count) {
            scaleSelect.value = "0";
            paintScale();
        }
    }
    function scaleSettings() {
        return [Number(byId("scale-root").value), Number(byId("scale-mode").value)];
    }
    function paintScale() {
        const [root, mode] = scaleSettings();
        const notes = chromaticNames.map((name, i) => {
            const cell = document.createElement("span");
            cell.textContent = name;
            cell.classList.toggle("in-scale", ZiviDSP.scaleNote(60 + i, root, mode) === 60 + i);
            return cell;
        });
        byId("scale-notes").replaceChildren(...notes);
    }
    for (const id of ["scale-root", "scale-mode"]) byId(id).addEventListener("change", () => {
        paintScale();
        send("scale-settings", ...scaleSettings());
    });
    const sequenceTabs = [...document.querySelectorAll("[data-seq]")];
    for (const button of sequenceTabs) button.addEventListener("click", () => {
        for (const tab of sequenceTabs) {
            const selected = tab === button;
            tab.setAttribute("aria-pressed", selected);
            byId("seq-" + tab.dataset.seq).hidden = !selected;
        }
        if (button.dataset.seq === "ks") renderKsTimeline(ksState?.position ?? 0);
    });
    paintScale();

    function paintStepTransport(step, previous) {
        const runningIndex = value => value?.playing ? value.index : null;
        const at = runningIndex(step), old = runningIndex(previous);
        if (at !== old) {
            stepButtons[old]?.classList.remove("playing");
            stepButtons[old]?.removeAttribute("aria-current");
            stepButtons[at]?.classList.add("playing");
            stepButtons[at]?.setAttribute("aria-current", "step");
        }
        const mode = step?.recording ? "record" : step?.playing ? "play" : step ? "stop" : "idle";
        const transport = byId("step-transport"), clock = byId("step-clock");
        if (transport.dataset.state !== mode) {
            transport.dataset.state = mode;
            transport.textContent = { record: "Gravando", play: "Reproduzindo", stop: "Parado", idle: "Sem leitura do SMK" }[mode];
        }
        clock.hidden = !step?.clock;
        if (step?.clock) {
            const { external, locked, bpm } = step.clock;
            const label = external && !locked ? "Aguardando clock externo"
                : (external ? "Clock externo" : "Clock interno") + " · " + bpm + " BPM";
            if (clock.textContent !== label) clock.textContent = label;
        }
    }

    function reportHardware(state) {
        const previous = hardwarePerformance;
        hardwarePerformance = state;
        const editing = Boolean(state?.editing);
        byId("step-play").textContent = editing ? "Tocar no SMK" : "Tocar no KS";
        byId("step-record").disabled = !editing;
        if (editing || previous?.editing) paintStepTransport(editing ? state.step : { playing: false }, previous?.step);
        if (!state?.editing) return;
        if (!state.sparse && (!previous?.editing || Object.keys(state.arp).some(key => state.arp[key] !== previous.arp[key]))) {
            arpEnabled = state.arp.enabled;
            byId("arp-bpm").value = state.arp.bpm;
            byId("arp-gate").value = state.arp.gate;
            byId("arp-order").value = state.arp.order;
            byId("arp-division").value = state.arp.division;
            byId("arp-hold").checked = state.arp.hold;
            byId("arp-free").checked = state.arp.freeRun;
            byId("arp-chance").value = state.arp.chance;
            byId("arp-chance-value").value = state.arp.chance + "%";
            byId("arp-toggle").setAttribute("aria-pressed", arpEnabled);
            byId("arp-toggle").textContent = arpEnabled ? "Desativar ARP" : "Ativar ARP";
            byId("arp-gate-value").value = state.arp.gate + "%";
            processor?.port.postMessage({ type: "arp-settings", values: [state.arp] });
        }
        if (!state.sparse && (!previous?.editing || state.scale.root !== previous.scale.root || state.scale.mode !== previous.scale.mode)) {
            byId("scale-root").value = state.scale.root;
            byId("scale-mode").value = state.scale.mode;
            paintScale();
            processor?.port.postMessage({ type: "scale-settings", values: [state.scale.root, state.scale.mode] });
        }
        paintArpNote(state.arpNote);
        updateKsStatus(state);
    }

    function phraseStatus() {
        return { mode: "stop", count: portableSequence?.events.length ?? 0, limit: "",
                 duration: (portableSequence?.lengthMs ?? 0) / 1000, position: 0 };
    }

    function updateKsStatus(state) {
        ksState = state;
        ksPhraseEditor.setRecording(state.mode === "record");
        const modes = { record: "Gravando", play: "Reproduzindo", stop: "Parado" };
        const limits = { time: " · limite de tempo", memory: " · memória cheia" };
        const phase = state.mode === "record" && !state.count ? "Armado" : modes[state.mode];
        const status = phase + " · " + state.count + " / 256 eventos" + (limits[state.limit] ?? "");
        if (byId("ks-status").textContent !== status) byId("ks-status").textContent = status;
        byId("ks-record").setAttribute("aria-pressed", String(state.mode === "record"));
        byId("ks-play").setAttribute("aria-pressed", String(state.mode === "play"));
        byId("ks-stop").disabled = state.mode === "stop";
        byId("ks-play").disabled = state.count === 0;
        byId("ks-progress").max = state.duration || 1;
        byId("ks-progress").value = Math.min(state.duration, state.position);
        byId("ks-time").textContent = ksTime.format(state.position) + " / " + ksTime.format(state.duration) + " s";
        const note = state.note == null ? "—" : chromaticNames[state.note % 12] + (Math.floor(state.note / 12) - 1);
        if (byId("ks-note").textContent !== note) byId("ks-note").textContent = note;
        if (!hardwarePerformance?.editing && playingStepGrid && state.mode === "play") {
            paintStepTransport({ playing: true }, null);
        } else if (!hardwarePerformance?.editing && state.mode === "stop" && byId("step-transport").dataset.state === "play") {
            playingStepGrid = false;
            paintStepTransport({ playing: false }, null);
        }
        if (!byId("seq-ks").hidden) renderKsTimeline(state.position);
    }

    for (const [id] of ksSettings) byId("ks-" + id).addEventListener("input", updateKsControls);
    byId("ks-loop").addEventListener("change", updateKsControls);
    ksTrigger.addEventListener("change", () => {
        markEdited();
        updateKsControls();
    });
    async function runKs(command) {
        if (command !== "stop" && muted) setMuted(false);
        const type = "ks-" + command;
        if (liveEdit.isEditing()) {
            liveEdit(type, []);
            return;
        }
        if (command !== "stop") await startAudio();
        if (!processor) return;
        if (command === "record") {
            ksRevision++;
            updateKsStatus({ mode: "record", count: 0, limit: "",
                             duration: readKsSettings().limitSeconds, position: 0 });
        }
        const values = command === "record" ? [ksRevision] : command === "play" ? [ksTrigger.value === "auto"] : [];
        processor.port.postMessage({ type, values });
    }

    for (const command of ["record", "play", "stop"])
        byId("ks-" + command).addEventListener("click", () => runKs(command));

    let randomHeld = false;
    async function randomPress() {
        randomHeld = true;
        await startAudio();
        if (randomHeld) {
            if (muted) setMuted(false);
            send("ks-random", true);
        }
    }
    function randomRelease() {
        randomHeld = false;
        send("ks-random", false);
    }
    const randomButton = byId("ks-random");
    randomButton.addEventListener("pointerdown", event => {
        if (event.button !== 0) return;
        randomButton.setPointerCapture(event.pointerId);
        randomPress();
    });
    for (const event of ["pointerup", "pointercancel", "lostpointercapture", "blur"]) {
        randomButton.addEventListener(event, randomRelease);
    }
    randomButton.addEventListener("keydown", event => {
        if (event.key !== " " && event.key !== "Enter") return;
        event.preventDefault();
        if (!event.repeat) randomPress();
    });
    randomButton.addEventListener("keyup", event => {
        if (event.key === " " || event.key === "Enter") randomRelease();
    });
    window.addEventListener("blur", randomRelease);

    function noteOn(owner, note, velocity) {
        sustainedOwners.delete(owner);
        noteOff(owner);
        if (!owner.startsWith("m:")) note = ZiviDSP.scaleNote(note, ...scaleSettings());
        inputNotes.set(owner, [note, velocity]);
        keys.get(note)?.classList.add("sounding");
        byId("note-status").textContent = "MIDI " + note;
        send("note", note, velocity);
    }

    function noteOff(owner) {
        sustainedOwners.delete(owner);
        const event = inputNotes.get(owner);
        if (!event) return;
        inputNotes.delete(owner);
        const [note] = event;
        let other = null;
        for (const value of inputNotes.values()) {
            if (value[0] === note) other = value;
        }
        if (other) return;
        keys.get(note)?.classList.remove("sounding");
        if (!inputNotes.size) byId("note-status").textContent = "C3–F5";
        else byId("note-status").textContent = "MIDI " + [...inputNotes.values()].at(-1)[0];
        send("note", note, 0);
    }

    function panic() {
        inputNotes.clear();
        midiPedals.clear();
        sustainedOwners.clear();
        keys.forEach(key => key.classList.remove("sounding"));
        byId("note-status").textContent = "C3–F5";
        send("notes-off");
    }
    function setMuted(state) {
        muted = state;
        if (muted) panic();
        send(muted ? "panic" : "resume");
        byId("panic").textContent = muted ? "Retomar áudio" : "Silenciar";
        byId("panic").setAttribute("aria-pressed", String(muted));
    }
    byId("panic").addEventListener("click", () => setMuted(!muted));
    window.addEventListener("blur", () => {
        for (const owner of inputNotes.keys()) {
            if (!owner.startsWith("m:")) noteOff(owner);
        }
    });

    const transposition = byId("ks-transpose");
    const transposePads = [];
    for (const [semitones, label] of [[7, "Fifth"], [4, "Third"], [2, "Tone"], [1, "½ Tone"], [0, "0"]]) {
        const button = document.createElement("button");
        button.type = "button";
        button.textContent = label;
        button.title = semitones ? "+" + semitones + " semitons no KS" : "Sem transposição";
        button.addEventListener("click", () => {
            transposition.value = semitones;
            transposition.dispatchEvent(new Event("input"));
        });
        byId("ks-transpose-pads").append(button);
        transposePads.push([semitones, button]);
    }
    function paintTransposePads() {
        for (const [semitones, pad] of transposePads)
            pad.setAttribute("aria-pressed", String(semitones === transposition.valueAsNumber));
    }
    transposition.addEventListener("input", paintTransposePads);
    let whiteKeys = 0;
    for (let note = 48; note <= 77; note++) {
        const name = noteName(note);
        const accidental = names[note % 12].length > 1;
        const key = document.createElement("button");
        key.type = "button";
        key.className = "key" + (accidental ? " accidental" : "");
        key.style.setProperty("--key-position", whiteKeys);
        key.textContent = name;
        key.setAttribute("aria-label", name);
        const shortcut = typing[note - 60];
        if (shortcut) {
            const label = document.createElement("small");
            label.textContent = shortcut.toUpperCase();
            key.append(label);
        }
        if (!accidental) whiteKeys++;
        keys.set(note, key);
        byId("keys").append(key);
        key.addEventListener("pointerdown", event => {
            if (event.button !== 0) return;
            key.setPointerCapture(event.pointerId);
            noteOn("p:" + event.pointerId, note, 108);
            startAudio();
        });
        for (const type of ["pointerup", "pointercancel", "lostpointercapture"]) {
            key.addEventListener(type, event => noteOff("p:" + event.pointerId));
        }
        key.addEventListener("keydown", event => {
            if (event.key !== " " && event.key !== "Enter") return;
            event.preventDefault();
            if (event.repeat) return;
            noteOn("v:" + note, note, 108);
            startAudio();
        });
        key.addEventListener("keyup", event => {
            if (event.key === " " || event.key === "Enter") noteOff("v:" + note);
        });
        key.addEventListener("blur", () => noteOff("v:" + note));
    }

    byId("keys").style.setProperty("--white-keys", whiteKeys);
    document.addEventListener("keydown", event => {
        if (event.repeat || event.metaKey || event.ctrlKey || event.altKey ||
            event.target.matches("input:not([type=range]), select, textarea")) return;
        const index = typing.indexOf(event.key.toLowerCase());
        if (event.key.length !== 1 || index < 0) return;
        event.preventDefault();
        noteOn("k:" + event.code, 60 + index, 108);
        startAudio();
    });
    document.addEventListener("keyup", event => noteOff("k:" + event.code));

    function midiMessage(input, data) {
        if (data.length !== 3) return;
        const [status, a, b] = data;
        const channel = (status & 15) + 1;
        const filter = Number(byId("channel").value);
        if (filter && channel !== filter) return;
        const prefix = "m:" + input.id + ":" + channel + ":";
        const owner = prefix + a;
        const release = () => {
            if (midiPedals.has(prefix) && inputNotes.has(owner)) sustainedOwners.add(owner);
            else noteOff(owner);
        };
        switch (status & 0xf0) {
        case 0x90:
            if (b) noteOn(owner, a, b);
            else release();
            break;
        case 0x80:
            release();
            break;
        case 0xe0: {
            const centered = (a | b << 7) - 8192;
            previewMidiEdit = true;
            positionStick(centered / (centered >= 0 ? 8191 : 8192));
            previewMidiEdit = false;
            break;
        }
        case 0xb0:
            if (![1, 7, 64, 120, 123].includes(a)) {
                const slot = midiCC.slice(0, 16).indexOf(a);
                if (slot >= 0) {
                    const page = midiBanks[slot >> 3].value;
                    if (page !== "") {
                        const dial = PanelCatalog[Number(page)].dials[slot & 7];
                        if (dial) knobs.get(dial.id).midi(b, knobFormat.value === "relative");
                        break;
                    }
                }
            }
            previewMidiEdit = true;
            if (a === 1 || a === 16 || a === 17) {
                const position = (b - 64) / (b >= 64 ? 63 : 64);
                if (a === 16) positionStick(position);
                else positionStick(undefined, position);
            } else if (a === 7) {
                const control = byId("level");
                control.value = b / 127;
                control.dispatchEvent(new Event("input"));
            }
            previewMidiEdit = false;
            if (a === 64 && b >= 64) midiPedals.add(prefix);
            else if (a === 64 || a === 120 || a === 123) {
                midiPedals.delete(prefix);
                const owners = a === 64 ? sustainedOwners : inputNotes.keys();
                for (const candidate of owners) if (candidate.startsWith(prefix)) noteOff(candidate);
            }
            break;
        }
    }

    for (let channel = 1; channel <= 16; channel++) {
        byId("channel").add(new Option(String(channel), String(channel)));
    }
    byId("channel").addEventListener("change", panic);

    function updateMIDI() {
        const inputs = [...midi.inputs.values()].filter(input => input.state === "connected");
        for (const input of inputs) {
            input.onmidimessage = event => midiMessage(input, event.data);
        }
        midiStatus.textContent = inputs.length ? inputs.map(input => input.name || "Entrada MIDI").join(", ")
            : "Nenhuma entrada MIDI disponível";
    }

    byId("midi").addEventListener("click", async () => {
        if (midi) return;
        try {
            if (!navigator.requestMIDIAccess) throw new Error("Web MIDI indisponível neste navegador");
            startAudio();
            midi = await navigator.requestMIDIAccess({ sysex: false });
            midi.onstatechange = ({ port }) => {
                if (port.type === "input" && port.state === "disconnected") {
                    const prefix = "m:" + port.id + ":";
                    for (const owner of inputNotes.keys()) if (owner.startsWith(prefix)) noteOff(owner);
                    for (const pedal of midiPedals) if (pedal.startsWith(prefix)) midiPedals.delete(pedal);
                }
                updateMIDI();
            };
            updateMIDI();
        } catch (error) {
            midiStatus.textContent = error.message;
        }
    });
    function receiveHardwarePatch(patch, sequenceOmitted) {
        const current = patchSnapshot();
        const matchingColors = current.pinColors?.filter(([row, col]) =>
            patch.pins.some(([r, c, pin]) => r === row && c === col && pin === 1));
        previewMidiEdit = true;
        try {
            return applyPatch({ ...patch, name: current.name, pinColors: matchingColors,
                sequence: sequenceOmitted ? current.sequence : patch.sequence }, "", true, true);
        } finally {
            previewMidiEdit = false;
        }
    }

    function receiveHardwareDelta(delta) {
        if (delta.previous) {
            const suppress = previewMidiEdit;
            previewMidiEdit = true;
            try {
                receiveHardwareChanges(delta.previous, delta.current);
            } finally {
                previewMidiEdit = suppress;
            }
            return;
        }
        const controls = {}, routes = [];
        for (const [id, value] of Object.entries(delta.controls)) {
            const input = parameters[id];
            if (Math.fround(input.valueAsNumber) === Math.fround(value)) continue;
            input.value = value;
            knobs.get(id).paint();
            controls[id] = value;
        }
        for (const [row, col, pin] of delta.pins) {
            const index = row * 16 + col;
            if (pins[index] === pin) continue;
            pins[index] = pin;
            if (pin !== 1) pinColors.delete(index);
            routes.push([row, col, pin]);
        }
        const changed = Object.keys(controls);
        if (!changed.length && !routes.length) return;
        if (changed.includes(exactSelect.value)) updateExactValue();
        if (changed.some(id => envelopeControls.has(id))) drawEnvelope();
        if (scopeActive && scopeMode.value === "filter" && changed.some(id => filterControls.has(id))) drawSignal();
        if (routes.length) updateMatrix();
        markEdited();
        processor?.port.postMessage({ type: "patch-delta", values: [controls, routes] });
    }

    function receiveHardwareChanges(previous, current) {
        const routeChanges = (before, after) => {
            const unpack = routes => new Map(routes.map(route =>
                route.length === 2 ? route : [route[0] * 16 + route[1], route[2]]));
            const old = unpack(before), next = unpack(after);
            return [...new Set([...old.keys(), ...next.keys()])].flatMap(index =>
                (old.get(index) ?? 0) === (next.get(index) ?? 0) ? [] : [[index, next.get(index) ?? 0]]);
        };
        const controls = Object.fromEntries(Object.entries(current.controls)
            .filter(([id, value]) => previous.controls[id] !== value));
        const mainRoutes = routeChanges(previous.pins, current.pins);
        receiveHardwareDelta({ controls, pins: mainRoutes.map(([index, pin]) => [index >> 4, index & 15, pin]) });

        let edited = false;
        const ksChanged = ksSettings.filter(([, key]) => previous.ks[key] !== current.ks[key]);
        for (const [id, key] of ksChanged) byId("ks-" + id).value = current.ks[key];
        if (previous.ks.loop !== current.ks.loop) byId("ks-loop").checked = current.ks.loop;
        if (previous.sequenceTrigger !== current.sequenceTrigger)
            byId("ks-trigger").value = current.sequenceTrigger ? "sequence" : "live";
        if (ksChanged.length || previous.ks.loop !== current.ks.loop || previous.sequenceTrigger !== current.sequenceTrigger) {
            updateKsControls();
            edited = true;
        }

        const ksPins = routeChanges(previous.ksRoutes, current.ksRoutes);
        for (const [col, value] of ksPins) {
            ksRoutes[col] = value;
            send("ks-route", col, value);
        }
        if (previous.ksMatrix !== current.ksMatrix) {
            ksMatrix.checked = current.ksMatrix;
            send("ks-matrix", current.ksMatrix);
        }
        if (ksPins.length || previous.ksMatrix !== current.ksMatrix) {
            updateKsRoutes();
            updateMatrix();
            edited = true;
        }
        const usbPins = routeChanges(previous.usbRoutes, current.usbRoutes);
        for (const [at, value] of usbPins) {
            usbRoutes[at] = value;
            usbRouteControls.get(at).value = value;
            processor?.port.postMessage({ type: "usb-route", values: [at >> 4, at & 15, value] });
        }
        if (usbPins.length) updateTryAudioRoutes();
        edited ||= usbPins.length > 0;
        if (current.midiCC.some((value, i) => value !== previous.midiCC[i])) {
            current.midiCC.forEach((value, i) => {
                if (value !== previous.midiCC[i]) midiCC[i] = value;
            });
            updateMidiCC();
            edited = true;
        }

        const settings = current.performance, prior = previous.performance;
        const arpKeys = ["enabled", "bpm", "gate", "order", "division", "chance", "hold", "freeRun"];
        for (const key of arpKeys) {
            if (settings[key] === prior[key]) continue;
            if (key === "enabled") arpEnabled = settings.enabled;
            else if (key === "hold") byId("arp-hold").checked = settings.hold;
            else if (key === "freeRun") byId("arp-free").checked = settings.freeRun;
            else byId("arp-" + key).value = settings[key];
        }
        if (arpKeys.some(key => settings[key] !== prior[key])) {
            changeArp();
            edited = true;
        }
        if (settings.root !== prior.root || settings.scale !== prior.scale) {
            if (settings.root !== prior.root) byId("scale-root").value = settings.root;
            if (settings.scale !== prior.scale) byId("scale-mode").value = settings.scale;
            paintScale();
            send("scale-settings", ...scaleSettings());
            edited = true;
        }

        const grid = current.stepGrid, beforeGrid = previous.stepGrid;
        let gridChanged = false;
        for (const key of ["length", "bpm", "timingBpm", "division"]) {
            if (grid[key] === beforeGrid[key]) continue;
            stepGrid[key] = grid[key];
            gridChanged = true;
        }
        grid.steps.forEach((step, i) => {
            for (const [field, value] of step.entries()) {
                if (value === beforeGrid.steps[i][field]) continue;
                stepGrid.steps[i][field] = value;
                gridChanged = true;
            }
        });
        if (gridChanged) {
            activeStep = Math.min(activeStep, stepGrid.length - 1);
            renderSteps();
            edited = true;
        }
        if (JSON.stringify(current.sequence) !== JSON.stringify(previous.sequence)) {
            replaceKsPhrase(current.sequence);
            edited = true;
        }
        if (edited) markEdited();
    }

    const ksTime = new Intl.NumberFormat(locale, { minimumFractionDigits: 2, maximumFractionDigits: 2 });
    function replaceKsPhrase(phrase) {
        portableSequence = phrase;
        ksFromGrid = false;
        playingStepGrid = false;
        ksRevision++;
        ksPhraseEditor.show(phrase);
        processor?.port.postMessage({ type: "ks-phrase", values: [phrase, readKsSettings(), ksRevision] });
        updateKsStatus(phraseStatus());
    }
    const ksPhraseEditor = setupKsPhraseEditor(byId, phrase => {
        replaceKsPhrase(phrase);
        markEdited();
        if (liveEdit.isEditing()) liveEdit("patch", [patchSnapshot()]);
    });

    liveEdit = setupSmkStudio(patchSnapshot, applyPatch, reportHardware, limitPerformanceOptions, loadStepGrid,
        receiveHardwarePatch, reportPanel, receiveHardwareDelta, loadLocalMatrixPreset);
    setupFirmwarePanel(liveEdit.lockFirmware);
    resetCustomSelection = setupCustomPresets(patchSnapshot, applyPatch, validatePatch);
    const studioLanguage = document.querySelector("[data-studio-language]");
    const handoffKey = "zivi-aks-studio-language-draft";
    studioLanguage?.addEventListener("click", event => {
        if (event.metaKey || event.ctrlKey || event.altKey || event.shiftKey) return;
        const language = studioLanguage.dataset.studioLanguage;
        studioLanguage.hash = location.hash || (instantPanel.hidden ? "" : "#try");
        try { localStorage.setItem("zivi-aks-language", language); } catch {}
        try {
            sessionStorage.setItem(handoffKey, JSON.stringify({
                language, pathname: studioLanguage.pathname, patch: patchSnapshot(),
                preset: byId("preset").value, trigger: ksTrigger.value
            }));
        } catch {}
    });
    let handoff;
    if (studioLanguage) try {
        const saved = JSON.parse(sessionStorage.getItem(handoffKey) ?? "null");
        if (saved?.language === locale && saved.pathname === location.pathname) {
            handoff = saved;
            sessionStorage.removeItem(handoffKey);
        }
    } catch {}
    drawMatrix();
    if (handoff && applyPatch(handoff.patch, handoff.preset)) {
        if (["auto", "live", "sequence"].includes(handoff.trigger)) ksTrigger.value = handoff.trigger;
    } else applyPatch({ ...presets.initial, version: 1 }, "initial");
    updateKsControls();
    connectAnalyser();
})();
