const ZiviFirmware = (() => {
    const { Updater, IS_BLUETOOTH, productOf, logicalImage, checkUfw, ufwEntries, hasLoader, sha256hex } = FeluccaFirmwareTransport;
    const stock = "SMK-37 Pro_016";
    const loader = "ota-SMK-37 Pro_900";
    const loaderFor = origin => origin === stock ? [loader, "ota-" + stock] : [loader];
    const projectIds = new Set([
        "SMK-37 Pro_9800", "SMK-37 Pro_98000", "SMK-37 Pro_98100", "SMK-37 Pro_98200",
        "SMK-37 Pro_98300", "SMK-37 Pro_98400", "SMK-37 Pro_98500",
        ...Object.values(FirmwarePackages).map(item => item.id),
    ]);
    const supported = id => id === stock || projectIds.has(id);
    const maxSize = 1024 * 1024;

    function describe(id) {
        if (id === stock) return "M‑VAVE V016 · SMK‑37 Pro";
        if (loaderFor(stock).includes(id)) return "Modo de atualização · " + id;
        const versions = Object.keys(FirmwarePackages).filter(version => FirmwarePackages[version].id === id);
        return versions.length ? "Felucca " + versions.reverse().join(" / ") + " · " + id : id;
    }

    async function validate(bytes) {
        if (bytes.length < 1024 || bytes.length > maxSize) throw new Error("Tamanho de arquivo .fwsc inválido.");
        const id = productOf(bytes);
        if (id === stock) throw new Error("Para restaurar o V016 original, use o caminho M‑VAVE indicado nesta seção.");
        if (!projectIds.has(id)) throw new Error("O pacote não pertence ao ziviSynthi AKS para SMK‑37 Pro.");
        const sha256 = await sha256hex(bytes);
        const version = Object.keys(FirmwarePackages).find(key => FirmwarePackages[key].id === id &&
            FirmwarePackages[key].sha256 === sha256);
        if (!version) throw new Error("SHA‑256 desconhecido. Selecione um pacote publicado pelo projeto.");
        const image = logicalImage(bytes);
        checkUfw(image);
        const parts = ufwEntries(image);
        const ota = parts[100], flash = parts[0];
        if (flash?.len !== 0x9d000 || !ota || ota.len >= 0x4f00 || !hasLoader(image.subarray(ota.off, ota.off + ota.len)))
            throw new Error("O mapa de flash ou o carregador não corresponde ao SMK‑37 Pro.");
        return { id, version, image, sha256 };
    }

    function ports(access) {
        const usb = port => port.state === "connected" && port.name && !IS_BLUETOOTH(port.name);
        const outputs = [...access.outputs.values()].filter(usb);
        const bare = port => port.name.replace(/\s+\d+$/, "");
        return [...access.inputs.values()].filter(usb).flatMap(input => {
            const exact = outputs.filter(output => output.name === input.name);
            const paired = exact.length ? exact : outputs.filter(output => bare(output) === bare(input));
            return paired.length === 1 ? [{ input, output: paired[0], key: JSON.stringify([input.id, paired[0].id]) }] : [];
        });
    }

    function operation(source, target, pending) {
        if (!source || !target) return "";
        if (loaderFor(pending?.origin).includes(source) && pending?.sha256 === target.sha256) return "Retomar gravação";
        if (source === stock) return "Instalar Felucca";
        if (!supported(source)) return "";
        if (source === target.id) return "Reinstalar firmware";
        return Number(source.split("_")[1]) > Number(target.id.split("_")[1]) ? "Reverter firmware" : "Atualizar firmware";
    }

    class Session extends Updater {
        constructor(access, selected) {
            super(access);
            this.midi = access;
            this.selected = selected;
            this.openLinks = new Set();
        }

        watch() {
            this.before = new Set(ports(this.midi).map(pair => pair.key));
        }

        unwatch() {
            this.before = null;
        }

        async find(filter) {
            const candidates = ports(this.midi).filter(pair => pair.key === this.selected ||
                (this.expected && (pair.key === this.loaderPort || !this.before?.has(pair.key))));
            let match = null;
            try {
                for (const { input, output, key } of candidates) {
                    this.access = { inputs: new Map([[input.id, input]]), outputs: new Map([[output.id, output]]) };
                    const expected = this.expected || this.resumeIdentity && [this.resumeIdentity];
                    const found = await super.find(id => (!filter || filter(id)) &&
                        (!expected || expected.includes(id.text)), input.name);
                    if (!found) continue;
                    this.openLinks.add(found.link);
                    if (match) throw new Error("Mais de um teclado respondeu. Deixe apenas o SMK‑37 Pro em atualização conectado.");
                    match = { ...found, key };
                }
                return match;
            } finally {
                this.access = this.midi;
            }
        }

        closeLinks() {
            for (const link of this.openLinks) link.close();
            this.openLinks.clear();
        }

        async identify() {
            try { return (await this.find())?.id.text; }
            finally { this.closeLinks(); }
        }

        async waitFor(filter, milliseconds) {
            this.expected = this.rebooting ? [this.target.id] : loaderFor(this.origin);
            try {
                const found = await super.waitFor(filter, milliseconds);
                if (found && !this.rebooting) this.loaderPort = found.key;
                if (found && this.rebooting) this.returnPort = found.key;
                return found;
            } finally {
                this.expected = null;
            }
        }

        async perform(source, target, pending, step) {
            const action = operation(source, target, pending);
            if (!action) throw new Error("A origem e o pacote selecionado não permitem esta operação.");
            const resuming = action === "Retomar gravação";
            this.origin = resuming ? pending.origin : source;
            this.target = target;
            try {
                if (resuming) {
                    this.rebooting = false;
                    this.resumeIdentity = source;
                    const done = await super.resume(target.image, (phase, ...args) => {
                        if (phase === "reboot") this.rebooting = true;
                        step(phase, ...args);
                    }, target.id);
                    if (!done) throw new Error("O carregador deixou de responder antes de confirmar a gravação.");
                    return target.id;
                }
                const verified = await this.find(id => id.text === source);
                if (!verified) throw new Error("A identidade do teclado mudou. Identifique o dispositivo novamente.");
                this.rebooting = false;
                return await this.stages(verified, target.image, target.id, (phase, ...args) => {
                    if (phase === "reboot") this.rebooting = true;
                    step(phase, ...args);
                });
            } finally {
                this.closeLinks();
                this.unwatch();
                this.expected = null;
                this.resumeIdentity = null;
            }
        }
    }

    return { Session, ports, describe, validate, operation, supported, maxSize };
})();

function setupFirmwarePanel(lockStudio) {
    const get = id => document.getElementById("firmware-" + id);
    const port = get("port"), select = get("package"), file = get("file"), run = get("run");
    const connect = get("connect"), probe = get("probe"), progress = get("progress");
    const confirm = get("confirm"), consent = get("consent"), confirmWrite = get("confirm-write");
    const key = "zivisynthiA.firmware.interrupted.v1";
    let access, source, target, busy = false, writing = false;
    let pending;
    try { pending = JSON.parse(sessionStorage.getItem(key)); } catch {}

    for (const version of Object.keys(EmbeddedFirmware))
        select.add(new Option("Felucca " + version + (version === Object.keys(FirmwarePackages)[0] ? " · atual" : " · anterior"), version),
            select.options.length - 1);

    function savePending(value) {
        pending = value;
        try {
            if (value) sessionStorage.setItem(key, JSON.stringify(value));
            else sessionStorage.removeItem(key);
        } catch {}
    }

    function status(message, indicator) {
        get("status").textContent = message;
        get("indicator").textContent = indicator || "SMK‑37 Pro · USB";
    }

    function setSource(id) {
        source = id;
        get("device").textContent = id ? ZiviFirmware.describe(id) : "Aguardando identificação";
    }

    function render() {
        const action = ZiviFirmware.operation(source, target, pending);
        run.textContent = action || "Preparar gravação";
        run.disabled = busy || !action;
        connect.disabled = busy || Boolean(access);
        probe.disabled = busy || !port.value;
        port.disabled = busy || !access;
        select.disabled = file.disabled = busy;
        get("export").disabled = writing;
        get("file-field").hidden = select.value !== "local";
        get("target").textContent = target ? "Felucca " + target.version + " · SHA‑256 conferido" : "Aguardando pacote";
    }

    function refreshPorts() {
        if (writing) return;
        const before = port.value;
        const available = ZiviFirmware.ports(access);
        port.replaceChildren(new Option(available.length ? "Selecionar porta" : "Nenhuma porta USB MIDI", ""));
        for (const pair of available) port.add(new Option(pair.input.name, pair.key));
        port.value = available.some(pair => pair.key === before) ? before : (available.length === 1 ? available[0].key : "");
        if (port.value !== before) setSource();
        render();
    }

    function preventClose(event) {
        event.preventDefault();
        event.returnValue = true;
    }

    async function selectPackage() {
        busy = true;
        target = undefined;
        progress.hidden = true;
        render();
        try {
            const version = select.value;
            const chosen = file.files[0];
            if (version === "local" && chosen?.size > ZiviFirmware.maxSize) throw new Error("O arquivo .fwsc excede 1 MiB.");
            if (version === "local" && chosen) target = await ZiviFirmware.validate(new Uint8Array(await chosen.arrayBuffer()));
            else if (EmbeddedFirmware[version]) target = await ZiviFirmware.validate(Uint8Array.from(atob(EmbeddedFirmware[version]), c => c.charCodeAt(0)));
            status(target ? "Confira o teclado e a versão de destino antes de gravar." : "Selecione o pacote .fwsc da versão desejada.");
        } catch (error) {
            status(error.message, "Verificar pacote");
        } finally {
            busy = false;
            render();
        }
    }

    connect.addEventListener("click", async () => {
        busy = true;
        render();
        try {
            if (!window.isSecureContext || !navigator.requestMIDIAccess)
                throw new Error("Web MIDI requer Chrome ou Edge em um contexto seguro. Abra o HTML localmente ou por localhost/HTTPS.");
            const midi = await navigator.requestMIDIAccess({ sysex: true });
            if (!midi.sysexEnabled) throw new Error("Autorize o acesso a mensagens MIDI SysEx.");
            access = midi;
            access.addEventListener("statechange", refreshPorts);
            refreshPorts();
            status("Selecione a porta USB do SMK‑37 Pro e identifique o teclado.");
        } catch (error) { status(error.message, "Verificar USB"); }
        finally { busy = false; render(); }
    });
    port.addEventListener("change", () => {
        setSource();
        status("Identifique o teclado na porta selecionada.");
        render();
    });
    probe.addEventListener("click", async () => {
        const selected = port.value;
        setSource();
        busy = true;
        render();
        status("Consultando o modelo e a versão pela porta USB…");
        try {
            const id = await new ZiviFirmware.Session(access, selected).identify();
            if (!id || port.value !== selected) throw new Error("O teclado não respondeu nesta porta. Confira a conexão USB.");
            setSource(id);
            const op = ZiviFirmware.operation(source, target, pending);
            if (ZiviFirmware.supported(source) || op) status("Identidade confirmada. Confira o pacote antes de gravar.", "Teclado identificado");
            else if (source.startsWith("ota-")) status("Para retomar por aqui, selecione o mesmo pacote de uma gravação iniciada nesta aba. Para outras recuperações, use o atualizador de origem.", "Modo de atualização");
            else status("A versão instalada exige o atualizador ou procedimento de recuperação da respectiva base.", "Origem incompatível");
        } catch (error) { status(error.message, "Verificar USB"); }
        finally { busy = false; render(); }
    });
    select.addEventListener("change", selectPackage);
    file.addEventListener("change", selectPackage);
    get("export").addEventListener("click", () => document.getElementById("export").click());
    consent.addEventListener("change", () => { confirmWrite.disabled = !consent.checked; });
    run.addEventListener("click", () => {
        consent.checked = false;
        confirmWrite.disabled = true;
        confirm.returnValue = "";
        get("confirm-target").textContent = ZiviFirmware.describe(source) + " → Felucca " + target.version + ".";
        confirm.showModal();
    });

    confirm.addEventListener("close", async () => {
        if (confirm.returnValue !== "write" || !consent.checked || busy || run.disabled) return;
        const original = source, selected = port.value;
        let unlock;
        busy = writing = true;
        render();
        try {
            unlock = lockStudio();
            window.addEventListener("beforeunload", preventClose);
            progress.hidden = false;
            progress.removeAttribute("value");
            const session = new ZiviFirmware.Session(access, selected);
            const result = await session.perform(original, target, pending, (phase, _detail, bytes, percent) => {
                const labels = {
                    start: "Preparando o carregador…", verify: "Verificando o pacote no teclado…",
                    loader: "Aguardando a porta do carregador…", mode: "Transferindo para o carregador…",
                    write: "Gravando firmware…", reboot: "Aguardando reinício e conferência da versão…",
                    done: "O teclado reiniciou e confirmou a identidade " + target.id + ".",
                };
                if (phase === "start") savePending({ origin: original, sha256: target.sha256 });
                if (phase === "write") progress.value = percent ?? Math.min(97, Math.floor(bytes * 100 / target.image.length));
                if (phase === "reboot") progress.removeAttribute("value");
                if (labels[phase]) status(labels[phase], "Gravação USB");
            });
            savePending(null);
            writing = false;
            refreshPorts();
            if (ZiviFirmware.ports(access).some(pair => pair.key === session.returnPort)) {
                port.value = session.returnPort;
                setSource(result);
            }
            progress.value = 100;
            status("Felucca " + target.version + " · identidade confirmada após o reinício.", "Gravação concluída");
        } catch (error) {
            const recovery = {
                stopped: "A transferência parou antes da confirmação.",
                lost: "O cabo USB se desconectou durante a operação.",
                noloader: "O carregador do Pro não apareceu.",
                noreturn: "A versão de destino não respondeu depois do reinício.",
                mismatch: "O teclado voltou com uma identidade diferente da versão selecionada.",
                badreq: "O carregador solicitou dados fora dos limites do pacote.",
            };
            setSource();
            progress.hidden = true;
            status((recovery[error.code] || error.message) +
                (pending ? " Identifique novamente o teclado. Para retomar no carregador, mantenha esta aba e use o mesmo pacote." : ""), "Verificar gravação");
        } finally {
            unlock?.();
            writing = busy = false;
            window.removeEventListener("beforeunload", preventClose);
            refreshPorts();
        }
    });
    render();
}
