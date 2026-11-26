function setupKsPhraseEditor(byId, applyPhrase) {
    const { validateSequence } = ZiviDSP;
    const form = byId("ks-phrase-editor");
    const length = byId("ks-phrase-length");
    const list = byId("ks-event-list");
    const status = byId("ks-edit-status");
    const file = byId("ks-phrase-file");
    const exportButton = byId("ks-export-phrase");
    const addNote = byId("ks-add-note");
    const addRelease = byId("ks-add-release");
    const apply = byId("ks-apply-phrase");
    const cancel = byId("ks-cancel-phrase");
    const importButton = byId("ks-import-phrase");
    let current, recording = false;
    const report = text => { status.textContent = text; };

    function actions() {
        length.disabled = apply.disabled = cancel.disabled = importButton.disabled = recording;
        exportButton.disabled = recording || !current;
        addNote.disabled = recording || list.rows.length >= 256;
        addRelease.disabled = addNote.disabled || !list.rows.length;
        for (const input of list.querySelectorAll("input, button")) input.disabled = recording;
        if (!recording) for (const row of list.rows) row.cells[3].firstElementChild.disabled = !row.cells[2].firstElementChild.valueAsNumber;
    }

    function append([time, note, velocity, attack = velocity > 0]) {
        const row = list.insertRow();
        for (const [caption, value, min, max] of [
            ["Tempo (ms)", time, 0, 29999], ["Nota MIDI", note, 0, 127], ["Vel. MIDI", velocity, 0, 127]
        ]) {
            const control = document.createElement("input");
            control.type = "number";
            control.required = true;
            control.setAttribute("aria-label", caption);
            control.min = min;
            control.max = max;
            control.value = value;
            row.insertCell().append(control);
        }
        const gate = document.createElement("input");
        gate.type = "checkbox";
        gate.checked = attack;
        gate.setAttribute("aria-label", "Ataque");
        row.insertCell().append(gate);
        const gain = row.cells[2].firstElementChild;
        gain.addEventListener("input", () => {
            gate.disabled = !gain.valueAsNumber;
            if (gate.disabled) gate.checked = false;
        });
        const remove = document.createElement("button");
        remove.type = "button";
        remove.textContent = "Retirar";
        remove.setAttribute("aria-label", "Retirar evento");
        remove.addEventListener("click", () => {
            const focus = row.nextElementSibling ?? row.previousElementSibling;
            row.remove();
            actions();
            (focus?.cells[0].firstElementChild ?? addNote).focus();
        });
        row.insertCell().append(remove);
        return row.cells[0].firstElementChild;
    }

    function rows() {
        return [...list.rows].map(row => [...row.cells].slice(0, 4).map(({ firstElementChild }, i) =>
            i === 3 ? firstElementChild.checked : firstElementChild.valueAsNumber));
    }

    function show(phrase) {
        current = phrase;
        length.value = phrase?.lengthMs ?? 1000;
        list.replaceChildren();
        for (const event of phrase?.events ?? []) append(event);
        actions();
        report("");
    }

    function insert(attack) {
        const last = rows().filter(event => Number.isInteger(event[0])).sort((a, b) => a[0] - b[0]).at(-1);
        const time = last ? Math.min(last[0] + 125, 29999) : 0;
        if (last && time <= last[0]) {
            report("A frase atingiu o limite de 30 segundos.");
            return;
        }
        length.value = Math.max(length.valueAsNumber || 1, time + 1);
        const input = append([time, last?.[1] ?? 60, attack ? 100 : 0, attack]);
        actions();
        input.focus();
    }

    addNote.addEventListener("click", () => insert(true));
    addRelease.addEventListener("click", () => insert(false));
    cancel.addEventListener("click", () => show(current));
    form.addEventListener("submit", event => {
        event.preventDefault();
        if (recording) return;
        const events = rows().sort((a, b) => a[0] - b[0]);
        const phrase = events.length ? validateSequence({ lengthMs: length.valueAsNumber, events }) : undefined;
        if (events.length && !phrase) {
            report("Use uma nota com ataque no primeiro evento e tempos distintos dentro da duração.");
            return;
        }
        applyPhrase(phrase);
        report(phrase ? "Frase KS aplicada." : "Frase KS removida.");
    });
    importButton.addEventListener("click", () => file.click());
    file.addEventListener("change", async () => {
        const chosen = file.files[0];
        file.value = "";
        if (!chosen || recording) return;
        const original = current;
        try {
            const phrase = chosen.size <= 65536 && validateSequence(JSON.parse(await chosen.text()));
            if (!phrase) throw new Error();
            if (recording || original !== current) return;
            applyPhrase(phrase);
            report("Frase KS importada.");
        } catch {
            report("Arquivo de frase KS inválido.");
        }
    });
    exportButton.addEventListener("click", () => {
        if (!current) return;
        const url = URL.createObjectURL(new Blob([JSON.stringify(current, null, 2)], { type: "application/json" }));
        const link = document.createElement("a");
        link.href = url;
        link.download = "zivisynthi-ks.json";
        link.click();
        setTimeout(() => URL.revokeObjectURL(url), 1000);
    });

    return {
        show,
        setRecording(state) {
            if (recording === state) return;
            recording = state;
            actions();
        }
    };
}
