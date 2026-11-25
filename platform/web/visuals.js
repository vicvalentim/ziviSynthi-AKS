const ZiviVisuals = (() => {
    const clamp = (value, lo, hi) => Math.max(lo, Math.min(hi, value));
    const amber = "#f2c15b";
    const blue = "#83c1e7";
    const axisColor = "#a8b9be";
    const gridColor = "#31424a";
    const frequencies = [0, 1, 2].map(maximumFractionDigits => new Intl.NumberFormat("pt-BR", { maximumFractionDigits }));
    const frequency = hz => hz > 0 ? frequencies[hz < 10 ? 2 : hz < 100 ? 1 : 0].format(hz) + " Hz" : "—";
    const dialFrequency = hz => hz >= 1000 ? frequencies[2].format(hz / 1000) + " kHz" : frequency(hz);

    // The service manual specifies calibration points, with an exponential taper between them.
    function makeVernier(points, marks) {
        const scale = points.map(([position, hz]) => [position, Math.log(hz)]);
        function interpolate(value, axis) {
            const other = 1 - axis;
            for (let i = 1; i < scale.length; i++) {
                const [left, right] = [scale[i - 1], scale[i]];
                if (value > right[axis] && i < scale.length - 1) continue;
                const mix = clamp((value - left[axis]) / (right[axis] - left[axis]), 0, 1);
                return left[other] + mix * (right[other] - left[other]);
            }
        }
        return {
            marks,
            position: hz => interpolate(Math.log(hz), 1),
            value: position => Math.exp(interpolate(position, 0))
        };
    }

    const verniers = {
        audio: makeVernier(VernierCatalog.audio, [1000, 10000]),
        low: makeVernier(VernierCatalog.low, [0.05, 500]),
        filter: makeVernier(VernierCatalog.filter, [1000, 10000])
    };
    const responseMid = VernierCatalog.response;
    const responseTaper = {
        position: value => value <= responseMid ? 0.5 * value / responseMid : 0.5 + 0.5 * (value - responseMid) / (1 - responseMid),
        value: position => position <= 0.5 ? 2 * position * responseMid : responseMid + 2 * (position - 0.5) * (1 - responseMid)
    };

    function knob(input, output, format, { log = false, vernier, taper, increment = Number(input.step) || 1 } = {}) {
        const lo = Number(input.min), hi = Number(input.max);
        const curve = vernier ?? taper;
        const position = curve ? () => curve.position(input.valueAsNumber)
            : log ? () => Math.log(input.valueAsNumber / lo) / Math.log(hi / lo)
            : () => (input.valueAsNumber - lo) / (hi - lo);
        const shell = document.createElement("span");
        const face = document.createElement("span");
        shell.className = "knob";
        face.className = "knob-face";
        face.setAttribute("aria-hidden", "true");
        input.replaceWith(shell);
        shell.append(face, input);
        const shortcutButtons = [];

        const setValue = value => {
            const previous = input.valueAsNumber;
            input.value = clamp(value, lo, hi);
            if (input.valueAsNumber !== previous) input.dispatchEvent(new Event("input", { bubbles: true }));
        };
        if (vernier) {
            const label = "Vernier · " + dialFrequency(lo) + " a " + dialFrequency(hi) +
                " · marcas em " + vernier.marks.map(dialFrequency).join(" e ");
            shell.title = label;
            input.setAttribute("aria-description", label);
            const shortcuts = document.createElement("div");
            shortcuts.className = "dial-shortcuts";
            for (const hz of vernier.marks) {
                const mark = document.createElement("i");
                mark.className = "dial-mark";
                mark.setAttribute("aria-hidden", "true");
                mark.style.setProperty("--mark-angle", (vernier.position(hz) - 0.5) * 270 + "deg");
                shell.append(mark);
                const shortcut = document.createElement("button");
                shortcut.type = "button";
                shortcut.textContent = dialFrequency(hz);
                shortcut.setAttribute("aria-label", input.getAttribute("aria-label") + " · " + shortcut.textContent);
                shortcut.addEventListener("click", () => setValue(hz));
                shortcuts.append(shortcut);
                shortcutButtons.push(shortcut);
            }
            shell.parentElement.append(shortcuts);
        }

        const change = (fraction, snap = false) => {
            const p = clamp(fraction, 0, 1);
            let value = p === 0 ? lo : p === 1 ? hi
                : curve ? curve.value(p) : log ? lo * (hi / lo) ** p : lo + p * (hi - lo);
            if (snap || input.step !== "any") value = lo + Math.round((value - lo) / increment) * increment;
            setValue(value);
        };

        let gesture;
        input.addEventListener("pointerdown", event => {
            if (event.button !== 0 || gesture) return;
            event.preventDefault();
            input.focus();
            input.setPointerCapture(event.pointerId);
            gesture = { id: event.pointerId, x: event.clientX, y: event.clientY, p: position() };
        });
        input.addEventListener("pointermove", event => {
            if (gesture?.id !== event.pointerId) return;
            const distance = event.clientX - gesture.x + gesture.y - event.clientY;
            change(gesture.p + distance / (event.shiftKey ? 640 : 145));
        });
        for (const type of ["lostpointercapture", "pointercancel", "pointerup"]) {
            input.addEventListener(type, event => {
                if (gesture?.id === event.pointerId) gesture = undefined;
            });
        }

        input.addEventListener("keydown", event => {
            const direction = { ArrowLeft: -1, ArrowDown: -1, ArrowRight: 1, ArrowUp: 1,
                              PageDown: -10, PageUp: 10 }[event.key];
            if (event.key === "Home" || event.key === "End") {
                event.preventDefault();
                change(Number(event.key === "End"));
            } else if (direction) {
                event.preventDefault();
                if (log || vernier) change(position() + direction / (event.shiftKey ? 600 : 120));
                else change(position() + direction * increment / (hi - lo), !taper);
            }
        });

        const paint = () => {
            const p = clamp(position(), 0, 1);
            const label = format(input.valueAsNumber);
            shell.style.setProperty("--sweep", p * 270 + "deg");
            face.style.setProperty("--turn", (p - 0.5) * 270 + "deg");
            output.textContent = label;
            if (vernier) {
                const active = vernier.marks.findLastIndex(hz => input.valueAsNumber >= hz * (1 - 1e-9));
                shortcutButtons.forEach((button, index) => button.classList.toggle("in-range", index === active));
            }
            input.setAttribute("aria-valuetext", curve ? label + " · posição " + frequencies[1].format(10 * p) : label);
        };
        const midi = (value, relative) => {
            if (!relative) change(value / 127, !curve && !log);
            else {
                const delta = value < 64 ? value : value - 128;
                const step = vernier || log ? 1 / 720 : increment / (hi - lo);
                change(position() + delta * step, !curve && !log);
            }
        };
        return { paint, midi };
    }

    function chart(canvas, min, max, ticks, yTicks) {
        const g = canvas.getContext("2d");
        if (!g) return null;
        const w = canvas.width, h = canvas.height;
        const x0 = 30, x1 = w - 11, y0 = 12, y1 = h - 23;
        const x = t => x0 + t * (x1 - x0);
        const y = a => y1 - (clamp(a, min, max) - min) / (max - min) * (y1 - y0);
        g.fillStyle = "#111719";
        g.fillRect(0, 0, w, h);
        g.font = "10px monospace";
        g.fillStyle = axisColor;
        g.strokeStyle = gridColor;
        g.lineWidth = 1;
        g.beginPath();
        g.textAlign = "right";
        for (const [level, label] of yTicks) {
            const sy = y(level);
            g.moveTo(x0, sy);
            g.lineTo(x1, sy);
            g.fillText(label, x0 - 4, sy + 3);
        }
        for (const [t, label] of ticks) {
            const sx = x(t);
            g.moveTo(sx, y0);
            g.lineTo(sx, y1);
            g.textAlign = t === 0 ? "left" : t === 1 ? "right" : "center";
            g.fillText(label, sx, h - 7);
        }
        g.stroke();
        return { g, x, y, y0, y1 };
    }

    function trace(graph, count, sample, color = amber) {
        if (!graph) return;
        const { g, x, y } = graph;
        g.beginPath();
        g.strokeStyle = color;
        g.lineWidth = 1.75;
        for (let i = 0; i < count; i++) {
            const t = i / (count - 1);
            const level = sample(t);
            if (i) g.lineTo(x(t), y(level));
            else g.moveTo(x(t), y(level));
        }
        g.stroke();
    }

    function filterDb(hz, rate, cutoff, response, drive, level) {
        // Resposta linear da seção bilinear, com os três polos e sua malha fechada.
        const w = Math.PI * hz / rate;
        const angle = Math.atan(Math.tan(w) / Math.tan(Math.PI * cutoff / rate));
        const amplitude = Math.cos(angle) ** 3;
        const phase = -3 * angle;
        const feedback = 9.1 * response * amplitude;
        const divisor = Math.hypot(1 + feedback * Math.cos(phase), feedback * Math.sin(phase));
        return 20 * Math.log10(level * drive * amplitude / divisor);
    }

    function signal(canvas, mode, rate, data, filter) {
        if (mode === "wave") {
            const frames = Math.min(data?.length ?? 1024, 1024);
            const ms = 1000 * frames / rate;
            const ticks = [[0, "0"], [0.25, ""], [0.5, ""], [0.75, ""], [1, ms.toFixed(1) + " ms"]];
            const graph = chart(canvas, -1, 1, ticks, [[-1, "−1"], [0, "0"], [1, "1"]]);
            if (data) trace(graph, Math.min(frames, 256), t => data[data.length - frames + Math.round(t * (frames - 1))]);
            return "Janela de " + ms.toFixed(1).replace(".", ",") + " ms · escala PCM ±1";
        }

        const responseMode = mode === "filter";
        const maxHz = Math.min(rate * 0.49, 20000);
        const displayMin = responseMode ? 20 : Math.max(20, rate / (2 * (data?.length ?? 1024)));
        const ratio = maxHz / displayMin;
        const ticks = [100, 1000, 10000].filter(hz => hz > displayMin * 1.45 && hz < maxHz / 1.4)
            .map(hz => [Math.log(hz / displayMin) / Math.log(ratio), hz < 1000 ? String(hz) : hz / 1000 + "k"]);
        ticks.unshift([0, Math.round(displayMin) + " Hz"]);
        ticks.push([1, Math.floor(maxHz / 1000) + "k"]);
        const maxDb = responseMode ? 40 : 0;
        const graph = chart(canvas, -84, maxDb, ticks,
            responseMode ? [[-80, "−80"], [-40, "−40"], [0, "0"], [40, "+40"]]
                         : [[-80, "−80"], [-60, "−60"], [-40, "−40"], [-20, "−20"], [0, "0"]]);
        const hzAt = t => displayMin * ratio ** t;
        if (responseMode) {
            if (filter.response >= 8 / 9.1) return "Auto-oscilação · regime não linear";
            const cutoff = clamp(filter.cutoff, 5, rate * 0.22);
            trace(graph, 220, t => filterDb(hzAt(t), rate, cutoff, filter.response, filter.drive, filter.level), blue);
            return "Ganho linear · " + frequency(cutoff);
        }
        if (data) trace(graph, 220, t => {
            const bin = hzAt(t) * 2 * data.length / rate;
            const i = Math.floor(bin);
            const a = Math.max(-84, data[i] ?? -84);
            const b = Math.max(-84, data[i + 1] ?? -84);
            return a + (b - a) * (bin - i);
        });
        return "FFT de 2.048 amostras · dBFS";
    }

    function envelopeCurve({ mode, attack, on, decay, off }) {
        const spans = mode === 0 ? [3 * attack, attack + decay, 5 * decay] : [attack, on, decay, mode === 2 ? off : 0];
        const hold = spans[0], release = hold + spans[1], end = release + spans[2];
        const duration = end + (spans[3] ?? 0);
        function valueAt(t) {
            if (mode === 0) {
                if (t < release) return 1 - Math.exp(-t / attack);
                return (1 - Math.exp(-release / attack)) * Math.exp(-(t - release) / decay);
            }
            if (t <= hold) return t / hold;
            if (t <= release) return 1;
            return Math.max(0, 1 - (t - release) / decay);
        }
        return { spans, duration, valueAt };
    }

    function envelope(canvas, controls, phase) {
        const { spans, duration, valueAt } = envelopeCurve(controls);
        const ticks = [[0, "0"], [0.5, ""], [1, Number(duration.toFixed(1)) + " ms"]];
        const graph = chart(canvas, 0, 1, ticks, [[0, "0"], [0.5, ""], [1, "1"]]);
        if (graph) {
            const { g, x, y0, y1 } = graph;
            const active = phase === 0 ? 3 : phase - 1;
            let start = 0;
            for (const [index, span] of spans.entries()) {
                const end = start + span / duration;
                if (active === index && span > 0) {
                    g.fillStyle = "#324749";
                    g.fillRect(x(start), y0, x(end) - x(start), y1 - y0);
                }
                if (span > 0) {
                    g.strokeStyle = "#4d656b";
                    g.beginPath();
                    g.moveTo(x(end), y0);
                    g.lineTo(x(end), y1);
                    g.stroke();
                }
                start = end;
            }
            trace(graph, 220, t => valueAt(t * duration));
        }
        return controls.mode === 0 ? "Gate · duração da nota ilustrativa"
             : controls.mode === 1 ? "Único · A / On / Decay"
             : "Repetir · A / On / Decay / Off";
    }

    return { knob, frequency, dialFrequency, verniers, responseTaper, signal, envelope, envelopeCurve, filterDb };
})();
