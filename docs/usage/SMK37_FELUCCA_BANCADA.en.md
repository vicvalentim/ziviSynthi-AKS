# ziviSynthi AKS · Controller and Web Studio manual

[Português](SMK37_FELUCCA_BANCADA.md) · [Installation](INSTALL.en.md) · [USB audio](ENTRADA_AUDIO_USB.md#english) · [Handbook presets](PRESETS_HANDBOOK.md#english)

Firmware 3.9.7 targets **M‑VAVE SMK‑37 Pro** on **fm1-base + felucca-bsp**. Its USB identity is `SMK-37 Pro_99600`.

## USB editor and personal presets

1. Connect the Pro over USB. Open Web Studio in Chrome or Edge on HTTPS or localhost. For a local build, run `make serve`.
2. Click **Autorizar USB MIDI**, allow SysEx, select the Pro input/output pair and click **Vincular SMK**.
3. Choose **Carregar do SMK** to start with the hardware patch, or **Aplicar patch e editar ao vivo** to send the browser patch and enable live editing. The hardware controls remain available.
4. Open **Presets** and use **Salvar como novo** to save a named patch. **Meus presets** stores patches in the current browser profile and site. Export them as JSON for use elsewhere.

The Studio remembers the verified ports and attempts to relink when USB returns. Choose the direction of the first transfer when resuming editing. Live sync covers synthesis parameters, pins, KS/USB routes, KS events, SEQ grid, ARP/SCALE and 24 CC assignments. Live sync sends changed bytes with a full baseline for a new or missed revision. Recording data is sent on completion. The Studio retains white/red full-gain pin metadata in its JSON while the route remains present.

**Controles do painel SMK** synchronizes master, synth and USB return levels, USB audio routing, MIDI channels and destination, clock source and output, KS clock reference and note output, USB Thru, relative encoder mode and pad LED mode. Bluetooth pairing remains a hardware-panel operation. The encoders default to absolute CC. Select relative mode in Studio or use **FX** on **SCALE → MIDI / AUDIO** in controller mode. Relative messages use two’s complement. Map the two knob banks to modules under **Mapeamento de CC**. Default assignments are OSC 1 for bank A and OSC 3 for B.

**Perfil DSP** switches between AKS and Legacy 3.9.4 for the session. On the Pro, use **SCALE → MIDI / AUDIO → KBANK + FX**. The calibrated AKS profile loads on startup. Use the original 3.9.4 patches and its legacy profile, or the 3.9.4 firmware, for comparisons against that version.

**Gravar preset do editor** saves the browser patch to USER 1–3. **Gravar estado do SMK** saves the hardware state. **Carregar slot** loads the slot into both the Pro and Studio. On the Pro, use **PATCH → KNOB BANK**, pads 1–3 and **REC** to save or **PARA** to load. Stop playback and recording and release keys before saving. Writes can briefly interrupt audio. The slots use alternating CRC-checked copies in `0xEE000–0xF3FFF`, with ownership checked before a sector is erased.

In **Patch Matrix**, use **SMK preset** to choose the active patch or an occupied **USER 1–3** slot, then select **Load into matrix**. Studio reads the resulting hardware patch and resumes live editing.

## Browser panel and matrix

The panel groups Noise Generator and Output Filter together. The matrix has source labels to the left, destinations above, columns A–P below, and rows 1–16 on the right. White and red pins use nominal digital gain. Green pins use 50%. The 21-pin tray represents the service set. [Pin matrix](MATRIX_REFERENCE.md#english)

OSC 1/2 verniers reach 261 Hz at 6, 2,088 Hz at 8, and 10 kHz at 10. OSC 3 reaches 63 Hz at 8 and 500 Hz at 10. Reference keys set 1 kHz or 10 kHz for OSC 1/2 and 0.05 Hz or 500 Hz for OSC 3. Filter frequency 5 corresponds to approximately 261 Hz during self-oscillation at maximum Response, with a 150.855 Hz digital cutoff parameter. Response 5 denotes the onset region. Calibration uses exponential interpolation between oscillator reference points. [Circuit audit](../development/AUDITORIA_CIRCUITO_AKS.md#english)

Column B drives the browser meter. **Control voltage** is bipolar with zero centered. Try B11 for the trapezoid or B15 for Stick X. **Signal levels** shows rectified peak. Try B4 for oscillator 2. **Monitor de sinal** displays stereo level in dBFS. The 240 × 240 LCD shows a B peak bar.

Drag the joystick, use arrow keys while focused, or center it with Home or **Centralizar**. **Range X/Y** adjusts attenuation. On the Pro, open **SCALE → MIDI / AUDIO** and hold **KBANK** while turning K1 for X or K2 for Y. In EMS mode, line 16 receives KS pitch and Stick Y is unavailable on that row.

## SEQ, KS and recording

**SEQ** opens a 1–64-step monophonic editor with 16 steps per page. Presets with KS phrases load an editable copy where the phrase fits. The import preserves attacks, rests, durations and legato. The current grid remains available when loading a preset without a phrase.

| Control | Action |
|---|---|
| K1 | Set active length. Extending again restores steps previously outside that length. |
| K2 / K3 / K4 / K5 | Note, velocity, tempo and cursor. |
| < / > | Page navigation. |
| Pads 1–16 / PARA | Toggle steps. A new step takes the last played note and velocity. |
| FBANK | Toggle STEP / LIVE. |
| REC in stopped STEP | Arm note entry. Play to advance, or hold a pad and play to fill that step. |
| REC in LIVE or during playback | Start a one-cycle take from keys, MIDI or ARP output. REC ends the take early and starts playback. |
| PLAY | Start or stop. |
| FX / KBANK | Import the KS phrase into SEQ / copy SEQ to KS. |
| REPEAT | Open KS. Inside KS, REC records, PLAY plays, < stops, PARA selects trigger source, REPEAT toggles looping and holding FX plays Random. |

An extra attack within an occupied LIVE step shares that step. KS export uses 1 ms resolution. SEQ retains frame timing. With **MIDI / AUDIO → K5 EXT**, SEQ and KS follow MIDI Clock. The Studio displays clock and transport while editing. Its SEQ controls can read/send a grid, clear a range or the entire grid, and store the SEQ in an occupied USER slot independently of the other patch fields. The library exports full grids and excerpts as JSON. Copy a fragment into a blank grid when using a different time base.

## ARP and SCALE

**ARP** opens its own page, **SCALE** selects tonic and scale. On SCALE, pads 1–12 select the tonic, K2 cycles 32 scales, K3 skips eight and PARA restores chromatic. Quantization applies to local notes including MIDI Out, and the arpeggiator applies it to external MIDI input.

| ARP control | Action |
|---|---|
| PARA | Enable or disable. |
| K1 / K2 / K3 / K4 | Tempo 30–300 BPM, order, gate 10–95%, division. |
| K5 | Chance 0–100%. Skipped pulses become rests in a recording. |
| Pads 1–8 | Division from quarter to thirty-second notes, including triplets. |
| Pad 9 | HOLD keeps the last group of notes. |
| Pad 10 | FREE cycles a scale octave without held notes. The random order draws from the scale. |
| REC | Capture emitted notes into SEQ using the active tempo, division and gate. |

## GLOBAL / MODULES

**GLOBE** opens the modules. Select via **<**, **>** or pads 1–10. The display identifies encoder, parameter and value. **FX** toggles the scope. **PARA** freezes the scope. Hold **REC** in GLOBAL for manual Attack.

| Pad | Module | K1–K4 | K5–K8 |
|---|---|---|---|
| 1 | Oscillator 1 | Frequency, sine shape, sine level, ramp level | Fine tune, sine/ramp mix, —, master |
| 2 | Oscillator 2 | Frequency, shape, pulse level, triangle level | —, —, —, master |
| 3 | Oscillator 3 | Frequency, shape, pulse level, triangle level | —, —, —, master |
| 4 | Noise Generator | Color, level, —, — | —, —, —, master |
| 5 | Ring Modulator | Level, —, —, — | —, —, —, master |
| 6 | Filter / Oscillator | Cutoff, Response, Drive, level | —, —, —, master |
| 7 | Envelope Shaper | Attack, On, Decay, Off | Signal level, trapezoid level, mode, trigger |
| 8 | Spring Reverb | Mix, level, time, — | —, —, —, master |
| 9 | Input Amplifiers | Input 1 level, Input 2 level, —, — | —, —, —, master |
| 10 | Output Channels | Level 1, tone 1, pan 1, level 2 | Tone 2, pan 2, patch level, master |

In Envelope Shaper, use **PARA + K8** for hardware master. F1 sets synth level before its limiter, F2 controls USB return, F3 filter cutoff and F4 reverb mix. K4 in **MIDI / AUDIO** duplicates F2. **REPEAT** there switches USB audio between direct monitor and matrix. USB capture has a separate limiter and remains independent of physical master. Both limiters use a −1 dBFS ceiling and about 2.9 ms lookahead per stage. The codec clock is nominally 44,118 Hz. USB audio is 44,100 Hz, stereo and 16-bit.

**PATCH** opens banks of 4 × 4 presets. Use **< / >** to change bank, pads to load, or K5 + PARA for cursor selection. **Overview → K4** cycles the 128 instruments. Bank Select 0 preserves Program Change 0–127. Send CC0 = 10 with Program Change 0–3 for USB FX. In the matrix, K5 selects row, K6 column, K7 pin type and FX magnification. Pads edit the selected 4 × 4 region from two hardware rows, with 01–08 above 09–16.

**BT** opens BLE MIDI. Turn K1 right or press PLAY to enable the radio. Press PLAY to scan, K6 to select, PARA to connect and < to disconnect. **>** opens **BLUETOOTH AUDIO**, with the same scan/select/connect controls. FX switches local/Bluetooth output. This build provides A2DP/SBC transmission.

<span id="leds-midi-and-profiling"></span>

## LEDs and MIDI

**SCALE → > → REC** cycles fixed, animated and off pad LEDs. Fixed is the startup setting and waits for the synth and its tail to become quiet. Lit modes cap brightness at one-eighth, send at intervals of at least 96 ms, require four PCM blocks ready, and defer while LCD DMA is busy.

**GLOBE + PATCH** toggles controller mode. MIDI destination, RX/TX channels, clock and USB-to-TRS Thru are in **MIDI / AUDIO**. **FBANK** toggles Thru there.

| Control | Bank A | Bank B |
|---|---|---|
| Keys 1–37 | Notes 48–84 initially. OCTAVE shifts by 12 semitones. | — |
| Pads 1–16, KBANK + FBANK to switch | Notes 36–51 | Notes 52–67 |
| K1–K8, KBANK to switch | CC16–23 | CC24–31 |
| F1–F4, FBANK to switch | CC70–73 | CC76–79 |
| Pitch bend / Modulation / Sustain | 14-bit center 8192 / CC1 / CC64 | — |
| SEQ PLAY | CC119 and Start/Stop | — |

Use PARA + K8 to send K8. Holding KBANK + K8 sends CC31. The panel buttons send 127 on press and 0 on release. CC102–119 map in order to FX, SCALE, CHORD, REPEAT, PARA, GLOBE, PATCH, BT, ARP, OCTAVE −, OCTAVE +, KBANK, FBANK, SEQ REC, <, >, SEQ and SEQ PLAY.

## Installation and recovery

Use the [installation guide](INSTALL.en.md). The updater checks model, SHA‑256, package CRCs and application size. It pauses Studio editing and verifies identity after restart. If an interrupted write leaves the device in the Pro bootloader, identify it again and resume with the same package. Use the [M‑VAVE recovery procedure](https://github.com/m-vave/smk37_open#updates-and-recovery) for the factory V016 or if the unit stops enumerating.

Felucca Pro uses VM starting at `0x9D000`. Open 0.8.2-alpha2 uses `0x72000`, within Felucca’s write area. Changing layouts requires Pro-specific recovery and a valid backup. Builds 3.9.1 and 3.9.2 share a USB identity and are distinguished in the editor by package SHA‑256. [Build and dependency guide](../development/GITHUB_RELEASE.en.md)
