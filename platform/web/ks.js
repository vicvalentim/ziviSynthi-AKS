class Ks {
    constructor(rate) {
        this.rate = Math.round(rate);
        this.events = [];
        this.mode = "stop";
        this.length = this.recordFrame = this.recordLimit = 0;
        this.limit = "";
        this.randomState = 0x4269d331;
        this.randomHeld = false;
        this.randomNote = 69;
        this.realtimeSpread = this.sequenceSpread = this.speed = 100;
        this.transpose = 0;
        this.limitSeconds = 12;
        this.loop = true;
        this.sequenceTrigger = false;
        this.rewind();
    }

    controls(values) {
        const ranges = { realtimeSpread: [25, 200], sequenceSpread: [25, 200],
                         speed: [25, 400], transpose: [-24, 24], limitSeconds: [1, 30] };
        for (const [name, [low, high]] of Object.entries(ranges)) {
            if (Number.isFinite(values?.[name])) {
                this[name] = Math.min(high, Math.max(low, Math.round(values[name])));
            }
        }
        for (const name of ["loop", "sequenceTrigger"]) {
            if (typeof values?.[name] === "boolean") this[name] = values[name];
        }
    }

    rewind() {
        this.position = this.nextEvent = this.velocity = 0;
        this.note = 69;
        this.attack = false;
    }

    stop() {
        if (this.mode === "record") {
            const last = this.events.at(-1);
            this.length = last ? Math.max(this.recordFrame, last.frame + 1) : 0;
        }
        this.mode = "stop";
        this.rewind();
    }

    load(sequence) {
        this.stop();
        this.length = Math.round(sequence.lengthMs * this.rate / 1000);
        this.events = sequence.events.map(([ms, note, velocity, attack]) => ({
            frame: Math.round(ms * this.rate / 1000), note, velocity, attack: attack ?? (velocity !== 0)
        }));
        this.limit = "";
        this.controls({ sequenceSpread: 100, speed: 100, transpose: 0, loop: true });
    }

    record() {
        this.stop();
        this.mode = "record";
        this.events.length = 0;
        this.length = this.recordFrame = 0;
        this.recordLimit = this.rate * this.limitSeconds;
        this.limit = "";
    }

    capture(note, velocity, attack) {
        if (this.mode !== "record") return;
        const last = this.events.at(-1);
        if (!last && !velocity) return;
        if (last && !attack && last.note === note && last.velocity === velocity) return;
        const event = { frame: this.recordFrame, note, velocity, attack };
        if (last?.frame === event.frame) {
            if (this.events.length === 1 && !velocity) this.events.pop();
            else {
                event.attack ||= last.attack && velocity !== 0;
                this.events[this.events.length - 1] = event;
            }
        } else if (this.events.length < 256) {
            this.events.push(event);
        } else {
            this.limit = "memory";
            this.stop();
        }
    }

    play() {
        this.stop();
        if (this.events.length === 0) return false;
        this.mode = "play";
        return true;
    }

    portableSequence() {
        if (this.mode === "record" || !this.events.length) return null;
        const toMs = frame => Math.round(1000 * frame / this.rate);
        const lengthMs = Math.max(1, Math.min(30000, toMs(this.length)));
        const events = [];
        for (const { frame, note, velocity, attack } of this.events) {
            const ms = Math.min(lengthMs - 1, toMs(frame));
            const last = events.at(-1);
            let triggered = attack;
            if (last?.[0] === ms) {
                events.pop();
                triggered ||= last[3] && velocity !== 0;
            }
            if (events.length || velocity) events.push([ms, note, velocity, triggered]);
        }
        if (!events[0]?.[3]) return null;
        const pairs = events.length % 2 === 0 && events.every((event, i) => i % 2
            ? event[2] === 0 && event[1] === events[i - 1][1]
            : event[3]);
        return { lengthMs, events: pairs ? events.map(event => event.slice(0, 3)) : events };
    }

    prepareSample() {
        this.attack = false;
        if (this.mode !== "play") return;
        const end = this.length * 100;
        if (this.position >= end) {
            if (!this.loop) {
                this.stop();
                return;
            }
            const position = this.position % end;
            this.rewind();
            this.position = position;
        }
        while (this.nextEvent < this.events.length && this.events[this.nextEvent].frame * 100 <= this.position) {
            const event = this.events[this.nextEvent++];
            this.note = event.note;
            this.velocity = event.velocity;
            this.attack = event.attack && event.velocity !== 0;
        }
    }

    advance() {
        if (this.mode === "play") this.position += this.speed;
        else if (this.mode === "record" && this.events.length && ++this.recordFrame === this.recordLimit) {
            this.limit = "time";
            this.stop();
        }
    }

    random(down) {
        if (down && !this.randomHeld) {
            let seed = this.randomState;
            seed ^= seed << 13;
            seed ^= seed >>> 17;
            seed ^= seed << 5;
            this.randomState = seed >>> 0;
            this.randomNote = 48 + this.randomState % 30;
        }
        this.randomHeld = down;
    }

    status() {
        const recording = this.mode === "record";
        return {
            mode: this.mode, count: this.events.length, limit: this.limit,
            duration: (recording ? this.recordLimit : this.length) / this.rate,
            position: (recording ? this.recordFrame : this.position / 100) / this.rate
        };
    }
}
