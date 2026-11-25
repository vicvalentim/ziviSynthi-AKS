# Web Studio protocol · Felucca Pro 3.9.5

[Português](WEB_STUDIO_PROTOCOL.md) · [Patch format](PATCH_FORMAT.md#english)

The editor uses Web MIDI SysEx over USB in a secure context with explicit authorization. Browser synthesis uses AudioWorklet and shares the C core’s catalogs. Personal presets use browser storage and export as JSON.

## Transport and patch image

A request starts with `F0 7D 5A 53 01 command serialLo serialHi`, followed by 7-bit payload and `F7`. A reply uses `command | 0x40`, echoes the serial number and adds status before payload. Status codes are 0 OK, 1 invalid, 2 incomplete and 3 CRC mismatch. The Studio expects a reply per command and batches rapid changes.

| Command | Function |
|---:|---|
| 1 | HELLO provides device model, version and the chunk limit of 96 bytes. |
| 2–4 | BEGIN with length and CRC-16/CCITT-FALSE, PART with offset, and COMMIT. |
| 5–7 | Write parameters, pins and KS settings. |
| 8 | Stop, play or record the active KS. |
| 9–10 | Read a snapshot in parts. The active phrase is unavailable during recording. |
| 11 | Manual Attack with a 750 ms deadline, joystick, ARP/SCALE configuration and transport telemetry. |

The image contains six quick controls, parameters in `parameters.def` order using five 7-bit bytes per IEEE 754 value, 256 pins in row order, ten KS settings bytes, duration in three 7-bit bytes, one format byte and up to 256 KS events. Each event carries time in milliseconds, note and velocity. Format 0 uses the original pairs. Format 1 encodes legato and uses time bit 20 for attack. The firmware validates bounds, CRC and payload before applying at the control loop. A transaction expires after two seconds without a part or on USB session change. Audio consumes PCM from a queue. USER storage writes require a separate command. The firmware drawer uses the Felucca update protocol and temporarily pauses Studio editing.

## Revisions 3.2 through 3.5

- **3.2, HELLO `98 4`.** Command 11 accepts legacy ARP payload `[2, enabled, bpmLo, bpmHi, gate, order]` plus optional division index 0–7, ordered 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T. The original 18-byte telemetry uses bits 2–4 of byte 10 for division and bits 0–1 for enabled and note present. SCALE keeps the payload and first six indexes.
- **3.3, HELLO `98 5`.** ARP order 3 selects random. Command 11 payload `[5, flags, chance]` uses flags bits 0 and 1 for HOLD and FREE and chance 0–100. Query `[6]` adds bytes 18 and 19 to the original telemetry. Byte 18 bits 0–3 signal HOLD, FREE, recording and playback. Byte 19 is chance. Query `[4]` still returns 18 bytes. KS phrases can be imported to SEQ and copied back to KS from the hardware panel.
- **3.5, HELLO `98 6`.** READ `[1]` returns both joystick axes and internal KS. Format bit 0 retains attack encoding, bit 1 selects the new format and bit 2 EMS mode. With bit 1 set, line 16 packs `pinType + 3 × ksRouteType` with each type 0–2. Older images with both bits clear migrate row 16 to internal routing. Old reads return effective KS routes on row 16. Command 11 uses `[8, valueLo, valueHi]` for Stick Y, `[9, column, type]` for an internal KS route and `[10, mode]` for internal (0) or EMS (1). Stick X remains subcommand 1.

## Extension in 3.8

HELLO returns `98 7`. READ `[2]` sets format bit 3 and appends the extension. READ with no argument or `[1]` remains available.

| Order | Field |
|---|---|
| 1 | 32 USB pins, left A–P then right A–P, each 0/1/2. |
| 2 | Nine ARP/SCALE bytes. Enabled/HOLD/FREE flags, 14-bit BPM, gate, order, division, chance, tonic and scale. |
| 3 | Six-byte SEQ header. Length, 14-bit BPM, 14-bit reference BPM and division. |
| 4 | 64 12-byte steps. Note, velocity, legato, extension, delay and duration. Each time uses 21 bits in DSP frames. |
| 5 | 24 CC assignments. K1–K8 banks A/B followed by F1–F4 banks A/B. |

Commands 12–15 address SEQ, USER slots, USB routes and CCs. For SEQ, send `[0, length, bpmLo, bpmHi, division]`, `[1, index, note, velocity]`, `[2, running]` or `[3]` to record. For USER slots, `[0]` lists, `[1, slot]` loads, and `[2, slot]` saves. Slots are zero-based 0–2. Command 14 takes `[channel, column, amount]`. Command 15 takes `[index, cc]`. Status codes 4 and 5 mean storage error and busy.

## Telemetry and independent SEQ transfers

In **3.9** (HELLO `98 8`), command 11 query `[7]` extends the 20-byte response to `[6]`. Byte 20 is playback step 0–63, shown when PLAY in byte 18 is set. Byte 21 carries source in bit 0 (external if 1) and sync in bit 1. Bytes 22–23 carry BPM. When external sync is absent, BPM reports the internal tempo. The Studio requests `[7]` on revision 8 or newer.

In **3.9.1** (HELLO `98 9`), the SEQ block is 774 bytes, a six-byte header plus 64 12-byte steps.

| Operation | Message |
|---|---|
| Read active grid | Command 12 with `[4]` returns size in two 7-bit bytes and CRC in three. Read the parts with command 10. Grid read is unavailable while recording. |
| Apply grid | BEGIN with size and CRC followed by type 1, then PART and COMMIT (3–4). The five-byte BEGIN form still targets the full patch. The firmware can return BUSY during KS or SEQ playback. |
| Save grid | Command 13 with `[3, slot]` updates SEQ in an occupied USER slot and preserves other fields. |

## Two-way editing in 3.9.3 and 3.9.4

In **3.9.3** (HELLO `98 10`), command 9 with `[3, sizeLo, sizeHi, crcLo, crcMid, crcHi]` compares the complete patch. A zero-length reply means equal state or recording in progress. Otherwise, read with command 10 and verify the CRC. Apply the snapshot once local writes are drained.

Command 16 with no payload reads 16 values, each in two 7-bit bytes. Write indices 0–13 with `[index, valueLo, valueHi]`. Command 11 subcommands 1 and 8 set the joystick axes. Red and white pins share the full-gain code. Red remains Studio metadata while that full-gain route stays active.

| Indices | Fields |
|---|---|
| 0–2 | Master, synth and USB return, 0–4096. |
| 3–6 | USB monitor/matrix, RX (0 for all), TX (1–16) and destination (all, TRS, USB, BLE). |
| 7–10 | External clock, clock output, KS reference 30–300 BPM and KS note output. |
| 11–13 | USB Thru, relative encoder CC and pad LED mode (fixed, animated, off). |
| 14–15 | Effective X/Y values, bipolar 14-bit centered on 8192. |

In **3.9.4** (HELLO `98 11`), command 17 takes the last 21-bit applied revision. Send zero for a full read. The reply starts with type and three revision bytes. Type 0 ends there (unchanged or deferred during transfer/recording). Type 1 sends controls with index 0–40 and five IEEE 754 bytes and pins with `[64, row, column, type]` where type 0 removes the pin. Type 2 sends the six-byte read header for command 10. It handles missed or structural revisions and deltas over 92 bytes. Accept a revision after resolving pending local edits.

Command 16 `[1]` includes X/Y range in Q12 and raw positions centered on 8192. Indices 16–17 are writable for the session. Command 11 `[11, axis, valueLo, valueHi]` adjusts raw position, with 0 for X and 1 for Y. Command 18 reads counters, and `[1]` resets before reading. The response is eight values of five 7-bit bytes each, in order.

1. Blocks filled.
2. Maximum block preparation time in µs.
3. Blocks at the nominal 2.901 ms deadline.
4. Minimum PCM reserve with synth active, or `0xffffffff` before a sample exists.
5. New MISS.
6. New DMA.
7. New USB DROP.
8. Maximum Studio command processing time in µs, excluding the diagnostic query.

## Sparse revisions in 3.9.5

HELLO returns `98 12`. Command 19 takes the last accepted revision in three 7-bit bytes. Zero requests a full baseline. Its reply starts with type and revision. Type 0 ends there. Types 1 and 2 add length (two bytes), CRC-16 (three bytes) and inline data up to 87 bytes. Read larger payloads with command 19, a two-byte offset and chunks up to 96 bytes.

- Type 2 contains the 42-byte panel, 475-byte base patch, 839-byte extension and any KS events at the end.
- Type 1 starts with the new KS length (two bytes). `0x3fff` keeps the old phrase. Changed spans contain a two-byte offset, one-byte count and only changed bytes. The new phrase, if any, is last.
- Panel field 20 represents the DSP profile. Send command 16 `[20, profile, 0]`, with 0 for Legacy and 1 for AKS.
- Command 20 sends a group followed by `[index, valueLo, valueHi]` triplets. Groups 0, 1 and 2 target KS settings, ARP/SCALE and SEQ header. The device validates before applying.
- Command 11 subcommand 7 accepts an optional three-byte CRC of the last telemetry and returns an empty payload on a match.

KS and SEQ recordings are forwarded on completion. Fixed-field offsets do not depend on KS event count. Patch, grid and USER storage formats remain compatible.
