class ZiviProcessor extends AudioWorkletProcessor {
    constructor() {
        super();
        this.synth = new ZiviDSP.Synth(sampleRate);
        this.framesUntilMeter = sampleRate / 24;
        this.peak = this.meter = 0;
        this.meterSum = this.meterFrames = 0;
        this.port.onmessage = ({ data }) => {
            const { type, values = [] } = data;
            if (type === "aks-profile") this.synth.modules.setProfile(Boolean(values[0]));
            if (type === "manual-attack") {
                const m = this.synth.modules;
                const down = Boolean(values[0]);
                if (down && !m.manualHold) m.manualAttack = true;
                m.manualHold = down;
            }
            if ((type === "stick-x" || type === "stick-y") && Number.isFinite(values[0])) {
                this.synth[type === "stick-x" ? "stickX" : "stickY"] = Math.max(-1, Math.min(1, values[0]));
            }
            if (type === "ks-route") this.synth.setKsRoute(...values);
            if (type === "ks-matrix") this.synth.setKsMatrix(Boolean(values[0]));
            if (type === "pin") this.synth.pin(...values);
            if (type === "note") this.synth.note(...values);
            if (type === "controls") this.synth.controls(values[0]);
            if (type === "patch-delta") {
                this.synth.controls(values[0]);
                for (const route of values[1]) this.synth.pin(...route);
            }
            if (type === "bend" && Number.isFinite(values[0])) {
                this.synth.bend = Math.max(-2, Math.min(2, values[0]));
            }
            if (type === "patch" && this.synth.applyPatch(values[0], values[1]) && !values[1]) this.capturing = false;
            if (type === "scale-settings") this.synth.configureScale(...values);
            if (type === "arp-settings") this.synth.configureArp(values[0]);
            if (type === "ks-phrase") {
                const [sequence, settings] = values;
                this.synth.loadPhrase(sequence);
                this.synth.ks.controls(settings);
                this.capturing = false;
            }
            if (type === "ks-controls") this.synth.ks.controls(values[0]);
            if (type === "ks-record") {
                this.synth.record();
                this.capturing = true;
            }
            if (type === "ks-play") this.synth.ks.play();
            if (type === "ks-stop") this.synth.ks.stop();
            if (type === "ks-random") this.synth.random(Boolean(values[0]));
            if (type === "notes-off") this.synth.panic();
            if (type === "panic") {
                this.synth.panic();
                this.synth.ks.stop();
                this.synth.modules.manualHold = this.synth.modules.manualAttack = false;
                // Um drone pode ignorar a envoltória. Pare a saída no limite do Worklet.
                this.muted = true;
                this.peak = this.meter = this.meterSum = this.meterFrames = 0;
            }
            if (type === "resume") this.muted = false;
        };
    }

    process(_inputs, outputs) {
        const [left, right] = outputs[0] ?? [];
        if (!left) return true;
        if (this.muted) {
            left.fill(0);
            right?.fill(0);
        } else {
            this.peak = Math.max(this.peak, this.synth.render(left, right));
            this.meter = Math.max(this.meter, this.synth.meterPeak);
            this.meterSum += this.synth.meterSum;
            this.meterFrames += left.length;
        }
        if (this.capturing && this.synth.ks.mode !== "record") {
            this.capturing = false;
            const ks = this.synth.ks;
            const sequence = ks.portableSequence();
            this.port.postMessage({ type: "ks-capture", sequence, eventCount: ks.events.length });
        }
        this.framesUntilMeter -= left.length;
        if (this.framesUntilMeter <= 0) {
            const matrixCv = this.meterFrames ? this.meterSum / this.meterFrames : 0;
            this.port.postMessage({ output: this.peak, matrix: this.meter, matrixCv, ks: this.synth.ks.status(),
                                    envelope: this.synth.modules.phase, contour: this.synth.modules.env,
                                    hz: this.synth.hz, arpNote: this.synth.arp.velocity ? this.synth.arp.note : null });
            this.peak = this.meter = 0;
            this.meterSum = this.meterFrames = 0;
            this.framesUntilMeter += sampleRate / 24;
        }
        return true;
    }
}
registerProcessor("zivisynthi", ZiviProcessor);
