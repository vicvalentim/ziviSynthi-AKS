const ZiviSequences = (() => {
    const { validateStepGrid } = ZiviDSP;
    const { emptyGrid } = ZiviStudioProtocol;
    const header = "zivisynthiA.seq.v1";

    function toKs(grid) {
        // Recorded SEQ timings are in SMK-37 Pro DSP frames, independent of the browser's rate.
        const rate = 44118;
        const triplet = grid.division % 2;
        const divisor = grid.bpm * 2 ** (grid.division >> 1) * (triplet ? 3 : 1);
        const period = Math.round(rate * (triplet ? 120 : 60) / divisor);
        const scale = frames => Math.round(frames * grid.timingBpm / grid.bpm);
        const steps = grid.steps.slice(0, grid.length).map(([note, velocity, legato, span, delay, duration]) => {
            const length = span ? Math.max(1, scale(span)) : period;
            const start = span ? Math.min(length - 1, scale(delay)) : 0;
            const end = span ? scale(delay + duration) : Math.floor(period * 3 / 4);
            return { note, velocity, legato, length, start, end: Math.min(length, Math.max(start + 1, end)) };
        });
        const frames = steps.reduce((sum, step) => sum + step.length, 0);
        if (frames > rate * 30 || !steps.some(step => step.velocity)) return null;
        const lengthMs = Math.round(frames * 1000 / rate);
        if (!lengthMs) return null;
        const events = [];
        function event(frame, note, velocity, attack) {
            const ms = Math.min(lengthMs - 1, Math.round(frame * 1000 / rate));
            const previous = events.at(-1);
            if (previous?.[0] === ms) {
                events.pop();
                attack ||= Boolean(velocity && previous[3]);
            }
            if (velocity || events.length) events.push([ms, note, velocity, attack]);
        }
        let offset = 0;
        for (const [i, step] of steps.entries()) {
            if (step.velocity) {
                event(offset + step.start, step.note, step.velocity, !step.legato || !events.at(-1)?.[2]);
                const next = steps[i + 1];
                const release = offset + step.end;
                if (release < frames && (step.end < step.length || !next?.velocity || next.start))
                    event(release, step.note, 0, false);
            }
            offset += step.length;
        }
        return ZiviDSP.validateSequence({ lengthMs, events });
    }

    function makeClip(grid, first, last) {
        if (first < 1 || first > last || last > grid.length) throw new RangeError("Selecione um trecho da sequência.");
        return { division: grid.division, timingBpm: grid.timingBpm, steps: grid.steps.slice(first - 1, last).map(step => step.slice()) };
    }

    function validateClip(clip) {
        if (!Array.isArray(clip?.steps) || !clip.steps.length || clip.steps.length > 64) return null;
        const grid = emptyGrid();
        grid.division = clip.division;
        grid.timingBpm = clip.timingBpm;
        grid.steps.splice(0, clip.steps.length, ...clip.steps);
        return validateStepGrid(grid) ? makeClip({ ...grid, length: clip.steps.length }, 1, clip.steps.length) : null;
    }

    function paste(grid, first, clip) {
        if (first + clip.steps.length - 1 > 64) throw new RangeError("O trecho ultrapassa o limite de 64 passos.");
        const next = validateStepGrid(grid);
        const active = next.steps.some(step => step[1] || step[3]);
        if (active && (clip.division !== next.division || clip.timingBpm !== next.timingBpm))
            throw new RangeError("O trecho usa outra divisão ou base de andamento. Cole em um SEQ vazio para conservá-las.");
        if (!active) {
            next.division = clip.division;
            next.timingBpm = clip.timingBpm;
            next.bpm = Math.max(next.division === 4 ? 40 : 30, next.bpm);
        }
        next.steps.splice(first - 1, clip.steps.length, ...clip.steps.map(step => step.slice()));
        next.length = Math.max(next.length, first + clip.steps.length - 1);
        return next;
    }

    function clear(grid, first, last) {
        const next = validateStepGrid(grid);
        const blank = emptyGrid().steps;
        next.steps.splice(first - 1, last - first + 1, ...blank.slice(first - 1, last));
        return next;
    }

    function validateEntry(entry) {
        const name = typeof entry?.name === "string" ? entry.name.trim().slice(0, 64) : "";
        if (!name) return null;
        const value = entry.kind === "seq" ? validateStepGrid(entry.value) :
            entry.kind === "clip" ? validateClip(entry.value) : null;
        return value ? { name, kind: entry.kind, value } : null;
    }

    function setup(byId, getGrid, setGrid, getFocus, render, syncKs) {
        const select = byId("step-shelf"), name = byId("step-name"), file = byId("step-file"), status = byId("step-status");
        const report = text => { status.textContent = text; };
        let saved = [], clip;
        try {
            const data = JSON.parse(localStorage.getItem(header) ?? "[]");
            if (Array.isArray(data)) saved = data.map(validateEntry).filter(Boolean);
        } catch { report("Não foi possível ler a biblioteca de sequências."); }
        const entryIndex = () => select.value === "" ? -1 : Number(select.value);
        const active = () => saved[entryIndex()];
        const interval = () => [byId("step-first").valueAsNumber, byId("step-last").valueAsNumber];
        function redraw(index = -1) {
            select.replaceChildren(new Option("Nova entrada", ""));
            saved.forEach((entry, i) => select.add(new Option((entry.kind === "clip" ? "Trecho · " : "SEQ · ") + entry.name, i)));
            select.value = index < 0 ? "" : String(index);
            const disabled = !active();
            for (const id of ["step-load", "step-update", "step-delete"]) byId(id).disabled = disabled;
        }
        function persist(next, index, feedback) {
            localStorage.setItem(header, JSON.stringify(next));
            saved = next;
            redraw(index);
            report(feedback);
        }
        function capture(kind) {
            const grid = getGrid();
            return { name: name.value, kind, value: kind === "clip" ? makeClip(grid, ...interval()) : grid };
        }
        function write(kind, index = -1) {
            const entry = validateEntry(capture(kind));
            if (!entry) {
                name.focus();
                throw new Error("Dê um nome à sequência ou ao trecho.");
            }
            const next = [...saved];
            next[index < 0 ? saved.length : index] = entry;
            persist(next, index < 0 ? saved.length : index, "Entrada salva no navegador.");
            if (entry.kind === "seq") syncKs(entry.value, status);
        }
        function setClip(value) {
            clip = value;
            byId("step-paste").disabled = false;
        }
        function action(id, callback) {
            byId(id).addEventListener("click", () => {
                try { callback(); }
                catch (error) { report(error.message); }
            });
        }
        function range() {
            const grid = getGrid(), [first, last] = interval();
            if (![first, last].every(Number.isInteger) || first < 1 || first > last || last > grid.length)
                throw new RangeError("Selecione um trecho da sequência.");
            return [first, last];
        }
        select.addEventListener("change", () => {
            const index = entryIndex();
            name.value = active()?.name ?? "";
            redraw(index);
        });
        action("step-copy", () => { setClip(makeClip(getGrid(), ...range())); report("Trecho copiado."); });
        action("step-paste", () => { if (clip) setGrid(paste(getGrid(), getFocus() + 1, clip), "Trecho colado no editor."); });
        action("step-clear-range", () => setGrid(clear(getGrid(), ...range()), "Trecho apagado no editor."));
        action("step-clear-grid", () => {
            if (window.confirm("Apagar os passos da sequência aberta no editor?"))
                setGrid(clear(getGrid(), 1, 64), "Sequência limpa no editor.");
        });
        action("step-save-grid", () => write("seq"));
        action("step-save-clip", () => { range(); write("clip"); });
        action("step-load", () => {
            const entry = active();
            if (!entry) return;
            if (entry.kind === "seq") {
                if (setGrid(entry.value, "Sequência carregada no editor.")) syncKs(entry.value, status);
            } else {
                setClip(entry.value);
                report("Trecho carregado. Selecione o passo inicial e use Colar.");
            }
        });
        action("step-update", () => {
            const entry = active();
            if (entry) {
                if (entry.kind === "clip") range();
                write(entry.kind, entryIndex());
            }
        });
        action("step-delete", () => {
            if (active() && window.confirm("Excluir esta entrada da biblioteca de sequências?"))
                persist(saved.filter((_, i) => i !== entryIndex()), -1, "Entrada excluída.");
        });
        action("step-export", () => {
            const entry = active() ?? validateEntry({ ...capture("seq"), name: name.value || "Sequência" });
            const link = document.createElement("a");
            const url = URL.createObjectURL(new Blob([JSON.stringify({ format: header, ...entry }, null, 2)], { type: "application/json" }));
            link.href = url;
            link.download = "ziviSynthiA-" + entry.kind + ".json";
            link.click();
            setTimeout(() => URL.revokeObjectURL(url), 1000);
        });
        action("step-import", () => file.click());
        file.addEventListener("change", async () => {
            const selected = file.files[0];
            file.value = "";
            if (!selected) return;
            try {
                if (selected.size > 65536) throw new Error("Arquivo de sequência acima de 64 KB.");
                const data = JSON.parse(await selected.text());
                const entry = data.format === header && validateEntry(data);
                if (!entry) throw new Error("Arquivo de sequência inválido.");
                persist([...saved, entry], saved.length, "Entrada importada para a biblioteca.");
                name.value = entry.name;
            } catch (error) { report(error.message); }
        });
        for (const id of ["step-first", "step-last"]) byId(id).addEventListener("change", render);
        redraw();
    }
    return { setup, makeClip, validateClip, paste, clear, toKs };
})();
