const assert = require("node:assert/strict");
const { pathToFileURL } = require("node:url");
const playwright = require(process.env.PLAYWRIGHT_MODULE || "playwright");

(async () => {
    const browser = await playwright.chromium.launch({
        executablePath: process.env.CHROMIUM_PATH,
        args: ["--no-sandbox"]
    });
    try {
        const page = await browser.newPage({ viewport: { width: 1040, height: 820 } });
        const errors = [];
        page.on("pageerror", error => errors.push(error.message));
        await page.goto(pathToFileURL(process.cwd() + "/build/web/index.html").href);
        assert.equal(await page.locator(".cell").count(), 256);
        assert.equal(await page.locator(".available").count(), 256);
        assert.equal(await page.locator(".key").count(), 30);
        assert.equal(await page.locator(".bank-section:visible").count(), 11);
        assert.equal(await page.locator(".source-label").count(), 16);
        assert.equal(await page.locator(".destination-label").count(), 16);
        assert.equal(await page.locator("#output-filter .knob").count(), 2);
        assert.equal(await page.locator("#faceplate").evaluate(el => getComputedStyle(el).display), "grid");
        assert.equal(await page.locator("#faceplate #bank-6 .knob").count(), 6);
        assert.equal(await page.locator("#bank-0").evaluate(el => getComputedStyle(el).gridArea), "osc1");
        assert.equal(await page.locator("#digital-controls .knob").count(), 7);
        assert.equal(await page.locator(".meter-jack").count(), 16);
        const meter = page.locator("#monitor-strip");
        const needle = page.locator("#vu-needle");
        assert.equal(await meter.getAttribute("data-mode"), "signal");
        await page.getByRole("radio", { name: "Control voltage" }).check();
        assert.equal(await meter.getAttribute("data-mode"), "cv");
        assert.equal(await needle.evaluate(el => el.style.getPropertyValue("--needle")), "0deg");
        await page.getByRole("radio", { name: "Signal levels" }).check();
        assert.equal(await meter.getAttribute("data-mode"), "signal");
        assert.equal(await needle.evaluate(el => el.style.getPropertyValue("--needle")), "-52deg");
        for (const width of [320, 360, 620, 621, 768, 900, 901, 1040, 1440]) {
            await page.setViewportSize({ width, height: 820 });
            const clipped = await page.evaluate(() => {
                const inside = (outer, inner) => inner.top >= outer.top - 1 && inner.bottom <= outer.bottom + 1 &&
                    inner.left >= outer.left - 1 && inner.right <= outer.right + 1;
                const failures = [];
                if (document.documentElement.scrollWidth > innerWidth) failures.push("página");
                for (const control of document.querySelectorAll(".faceplate .control, #digital-controls .control")) {
                    const module = control.closest(".bank-section, .stick-module, .digital-tools");
                    const id = control.querySelector("input").id;
                    if (!inside(module.getBoundingClientRect(), control.getBoundingClientRect())) failures.push(id);
                    for (const part of control.children)
                        if (!inside(control.getBoundingClientRect(), part.getBoundingClientRect())) failures.push(id + " " + part.tagName);
                }
                return failures;
            });
            assert.deepEqual(clipped, [], width + " px");
        }
        await page.setViewportSize({ width: 1040, height: 820 });
        const osc3 = page.locator("#bank-2");
        await osc3.getByRole("button", { name: /0,05 Hz/ }).click();
        assert.equal(await page.locator("#lfo").inputValue(), "0.05");
        await osc3.getByRole("button", { name: /500 Hz/ }).click();
        assert.equal(await page.locator("#lfo").inputValue(), "500");
        assert.equal(await page.locator("#preset-drawer").getAttribute("open"), null);
        await page.getByRole("link", { name: "Presets", exact: true }).click();
        assert.notEqual(await page.locator("#preset-drawer").getAttribute("open"), null);
        assert.equal(await page.locator("#firmware-drawer").getAttribute("open"), null);
        await page.getByRole("link", { name: "Instalar firmware", exact: true }).click();
        assert.notEqual(await page.locator("#firmware-drawer").getAttribute("open"), null);
        await page.getByLabel("Pacote de firmware").selectOption("local");
        assert.ok(await page.locator("#firmware-file").isVisible());
        assert.ok(await page.locator("#firmware-run").isDisabled());
        await page.locator("#firmware-drawer summary").click();
        assert.equal(await page.locator("#firmware-drawer").getAttribute("open"), null);
        await page.getByRole("button", { name: "Repor inicial" }).click();
        for (const bank of ["performance", "texture", "sequence"])
            assert.equal(await page.locator("#factory-" + bank + " option").count(), 16);
        await page.evaluate(() => {
            navigator.requestMIDIAccess = () => { throw new Error("Try requested MIDI access"); };
        });
        await page.getByRole("button", { name: "Experimentar / Try" }).click();
        await page.waitForFunction(() => document.activeElement.getAttribute("aria-label") === "C4");
        assert.ok(await page.locator("#audio").isDisabled());
        assert.equal(await page.locator("#midi-status").textContent(), "MIDI desconectado");
        await page.keyboard.down("a");
        await page.waitForFunction(() => document.querySelector("#meter").value > 0.10);
        assert.equal(await page.locator("#meter-reading").textContent(), "0%");
        await page.getByRole("button", { name: /B15 · STICK X/ }).click();
        for (let i = 0; i < 6; i++) await page.locator("#stick-pad").press("ArrowLeft");
        await page.getByRole("radio", { name: "Control voltage" }).check();
        await page.waitForFunction(() => document.querySelector("#meter-reading").value === "-60%");
        await page.getByRole("radio", { name: "Signal levels" }).check();
        await page.waitForFunction(() => document.querySelector("#meter-reading").value === "60%");
        await page.getByRole("button", { name: "Retirar", exact: true }).click();
        await page.getByRole("button", { name: /B15 · STICK X/ }).click();
        await page.getByRole("button", { name: "Branco", exact: true }).click();
        if (process.env.SCREENSHOT_PATH) await page.screenshot({ path: process.env.SCREENSHOT_PATH });

        const out = page.getByRole("button", { name: /A12 · ENV SIGNAL/ });
        await out.click();
        await page.getByRole("button", { name: /C12 · ENV SIGNAL/ }).click();
        await page.waitForFunction(() => document.querySelector("#meter").value === 0);
        assert.equal(await out.getAttribute("aria-pressed"), "false");
        await page.getByRole("button", { name: /Edit · 4 × 4/ }).click();
        assert.equal(await page.locator(".cell").count(), 16);
        assert.match(await page.locator("#region-status").textContent(), /A9 – D12/);
        await out.click();
        await page.waitForFunction(() => document.querySelector("#meter").value > 0.10);
        await page.getByRole("button", { name: /Região M13 a P16/ }).click();
        assert.match(await page.locator("#region-status").textContent(), /M13 – P16/);
        await page.getByRole("button", { name: /Overview · 16 × 16/ }).click();
        assert.equal(await page.locator(".cell").count(), 256);
        await page.getByRole("combobox", { name: "Escolher patch" }).selectOption("filter");
        assert.equal(await page.getByRole("button", { name: /H3 · OSC 1/ }).getAttribute("aria-pressed"), "true");
        await page.keyboard.up("a");
        await page.waitForFunction(() => document.querySelector("#meter").value < 0.002);
        await page.getByRole("button", { name: "Repor inicial" }).click();
        assert.equal(await out.getAttribute("aria-pressed"), "true");

        await page.getByRole("combobox", { name: "Escolher patch" }).selectOption("ks");
        await page.locator(".ks-options > summary").click();
        await page.getByText("Roteamento de afinação KS").click();
        assert.equal(await page.getByLabel(/I · OSC 1 FREQ/).inputValue(), "1");
        assert(await page.getByRole("button", { name: /I16 · STICK Y/ }).isVisible());
        await page.locator("#ks-matrix").check();
        assert(await page.getByRole("button", { name: /I16 · KS PITCH/ }).isVisible());
        assert(await page.locator("#stick-range-y").isDisabled());
        assert.match(await page.locator("#stick-route").textContent(), /KS/);
        await page.locator("#ks-matrix").uncheck();
        await page.getByText("Roteamento de afinação KS").click();
        await page.getByRole("button", { name: "Record", exact: true }).click();
        await page.waitForFunction(() => document.querySelector("#ks-status").textContent.startsWith("Armado"));
        await page.keyboard.down("a");
        await page.waitForFunction(() => document.querySelector("#meter").value > 0.10);
        await page.keyboard.up("a");
        await page.getByRole("button", { name: "Stop", exact: true }).click();
        await page.waitForFunction(() => /Parado · 2 \/ 256/.test(document.querySelector("#ks-status").textContent));
        await page.getByRole("button", { name: "Play", exact: true }).click();
        await page.waitForFunction(() => document.querySelector("#ks-status").textContent.startsWith("Reproduzindo"));
        await page.getByRole("button", { name: "Stop", exact: true }).click();

        await page.getByRole("combobox", { name: "Escolher patch" }).selectOption("spring");
        assert.equal(await page.getByRole("button", { name: /G12 · ENV SIGNAL/ }).getAttribute("aria-pressed"), "true");
        assert.equal(await page.getByRole("button", { name: /C14 · REVERB/ }).getAttribute("aria-pressed"), "true");
        await page.getByRole("button", { name: "Attack", exact: true }).click();
        await page.waitForFunction(() => document.querySelector("#envelope-meter").value > 0.05);

        await page.getByText("Edição numérica").click();
        await page.locator("#exact-param").selectOption("cutoff");
        await page.locator("#exact-value").fill("1870");
        await page.getByRole("button", { name: "Aplicar valor" }).click();
        assert.equal(await page.locator("#cutoff").inputValue(), "1870");
        await page.getByRole("button", { name: "Scale", exact: true }).click();
        assert.equal(await page.locator("#scale-mode option").count(), 32);
        await page.locator("#scale-mode").selectOption("1");
        assert.equal(await page.locator("#scale-notes .in-scale").count(), 7);
        await page.getByRole("button", { name: "ARP", exact: true }).click();
        await page.getByRole("button", { name: "Ativar ARP" }).click();
        assert.equal(await page.locator("#arp-toggle").getAttribute("aria-pressed"), "true");
        await page.getByRole("button", { name: "Desativar ARP" }).click();

        assert.equal(await page.locator("#bank-5 .encoder").allTextContents().then(items => items.join(" ")), "K1 K2 K4");
        assert.match(await page.locator("#digital-controls").innerText(), /Disparo ENV/i);
        await page.getByRole("button", { name: "SEQ", exact: true }).click();
        assert.equal(await page.locator("#step-grid button:visible").count(), 16);
        await page.getByLabel("Comprimento").fill("32");
        await page.getByLabel("Comprimento").press("Tab");
        await page.getByRole("button", { name: "17–32" }).click();
        await page.getByRole("button", { name: "Ligar ou silenciar passo" }).click();
        await page.getByRole("button", { name: "Copiar trecho" }).click();
        assert.equal(await page.getByRole("button", { name: "Colar no passo em foco" }).isEnabled(), true);
        await page.getByRole("button", { name: "Apagar trecho" }).click();
        assert.match(await page.locator("#step-status").textContent(), /Trecho apagado/);
        const response = page.locator("#response");
        await response.focus();
        await response.press("ArrowUp");
        assert.equal(await response.inputValue(), "0.01");

        await page.locator("#trace-mode").selectOption("filter");
        await page.waitForFunction(() => document.querySelector("#signal-scale").textContent.includes("Ganho linear"));
        await page.locator("#trace-mode").selectOption("spectrum");
        await page.waitForFunction(() => document.querySelector("#signal-scale").textContent.includes("FFT"));
        await page.getByRole("button", { name: "Pausar", exact: true }).click();
        assert(await page.locator("#trace-mode").isDisabled());
        await page.getByRole("button", { name: "Retomar", exact: true }).click();
        await page.getByRole("combobox", { name: "Escolher patch" }).selectOption("repeat");
        await page.waitForFunction(() => document.querySelector("#envelope-scale").textContent.startsWith("Repetir"));
        await page.waitForFunction(() => document.querySelector("#freq-4").textContent.includes("Hz"));
        assert.match(await page.locator("#freq-4").getAttribute("data-modulated"), /true/);

        for (const width of [360, 390, 768, 1280]) {
            await page.setViewportSize({ width, height: 800 });
            assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), width + "px");
        }
        assert.deepEqual(errors, []);
        console.log("Chromium: HTML local, áudio, teclado, matriz, KS, envoltória manual e layout móvel aprovados.");
    } finally {
        await browser.close();
    }
})().catch(error => {
    console.error(error);
    process.exitCode = 1;
});
