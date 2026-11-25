"""Exercise the USB editor wire in the actual Felucca host application."""
import argparse
import binascii
import math
from pathlib import Path
import struct
import subprocess
import sys
import wave

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from build_presets import load


def septets(value, size):
    return [(value >> (7 * i)) & 127 for i in range(size)]


def encode_float(value):
    return septets(struct.unpack("<I", struct.pack("<f", value))[0], 5)


def encode_patch(patch, *, xy=True):
    values = [item for v in patch["controls"].values() for item in encode_float(v)]
    matrix = [0] * 256
    for row, col, kind in patch["pins"]:
        matrix[row * 16 + col] = kind
    for col, amount in patch.get("ksRoutes", []):
        matrix[240 + col] += (3 if xy else 1) * amount
    ks = [100, 0, 100, 0, 100, 0, 24, 12, 1, int(patch["sequenceTrigger"])]
    sequence = patch.get("sequence", {"lengthMs": 0, "events": []})
    states = bool(sequence["events"] and len(sequence["events"][0]) == 4)
    events = []
    for ms, note, vel, *attack in sequence["events"]:
        events.extend([*septets(ms | ((1 << 20) if attack and attack[0] else 0), 3), note, vel])
    return values + matrix + ks + septets(sequence["lengthMs"], 3) + [int(states) | (2 if xy else 0) | (4 if patch.get("ksMatrix") else 0)] + events


def encode_extended(patch, *, note=60, velocity=0, length=16, bpm=120):
    wire = encode_patch(patch)
    wire[len(patch["controls"]) * 5 + 269] |= 8
    usb = [0] * 32
    for channel, col, value in patch.get("usbRoutes", []):
        usb[channel * 16 + col] = value
    perf = [0, *septets(120, 2), 60, 0, 2, 100, 0, 0]
    grid = [length, *septets(bpm, 2), *septets(bpm, 2), 4,
            note, velocity, *([0] * 10), *([0] * 63 * 12)]
    return wire + usb + perf + grid + list(range(16, 32)) + [70, 71, 72, 73, 76, 77, 78, 79]


def script_for(output):
    signature = [0xf0, 0x7d, 0x5a, 0x53, 1]
    lines = ["wait 250"]
    serial = 0

    def message(prefix, command, data):
        return prefix + " " + bytes([*signature, command, *septets(serial, 2), *data, 0xf7]).hex(" ").upper()

    def command(op, data=(), status=0, answer=()):
        nonlocal serial
        serial += 1
        lines.append(message("sysex", op, data))
        lines.append(message("expect_sysex", op | 64, [status, *answer]))

    def upload(wire, checksum=None, status=0):
        if checksum is None:
            checksum = binascii.crc_hqx(bytes(wire), 0xffff)
        command(2, septets(len(wire), 2) + septets(checksum, 3))
        for offset in range(0, len(wire), 96):
            command(3, septets(offset, 2) + wire[offset:offset + 96])
        command(4, status=status)

    command(1, answer=[1, 98, 12, 96])
    command(5, [5, *encode_float(1770)])
    lines.append("expect zcut 1770")
    command(5, [5, *encode_float(2222), 120, *encode_float(1)], status=1)
    lines.append("expect zcut 1770")
    command(6, [11, 0, 0])
    lines.append("expect zpin 0")
    command(6, [16, 0, 1], status=1)
    command(6, [11, 0, 1])
    lines.append("expect zpin 1")
    command(4, status=2)
    catalog = load(felucca=True)
    phrase = catalog["passos-ks"].patch
    wire = encode_patch(phrase)
    upload(wire, checksum=0, status=3)
    lines.append("expect zcut 1770")
    corrupt = wire.copy()
    corrupt[len(phrase["controls"]) * 5 + 14 * 16] = 3
    upload(corrupt, status=1)
    lines.append("expect zcut 1770")
    upload(wire)
    lines += ["expect zcut 970", "expect zksevents 16", "expect zksmode 0", "expect zseqlength 8", "expect zseqvel >0"]
    crc = binascii.crc_hqx(bytes(wire), 0xffff)
    command(9, [1], answer=[*septets(len(wire), 2), *septets(crc, 3), 0])
    for offset in range(0, len(wire), 96):
        command(10, septets(offset, 2), answer=wire[offset:offset + 96])
    upload(encode_patch(phrase, xy=False))
    lines += ["expect zksosc 1", "expect zksmatrix 0"]
    both = {**phrase, "ksMatrix": True, "pins": [*phrase["pins"], [15, 8, 2]]}
    xy_wire = encode_patch(both)
    upload(xy_wire)
    lines += ["expect zksosc 1", "expect zksmatrix 1"]
    command(9, [1], answer=[*septets(len(xy_wire), 2), *septets(binascii.crc_hqx(bytes(xy_wire), 0xffff), 3), 0])
    for offset in range(0, len(xy_wire), 96):
        command(10, septets(offset, 2), answer=xy_wire[offset:offset + 96])
    command(11, [10, 0])
    command(11, [9, 8, 2])
    lines += ["expect zksmatrix 0", "expect zksosc 2",
              "wheel mod 64", "expect zsticky 0", "wheel pitch -8192", "expect zstick -1000",
              "expect zsticky 0", "wheel mod 127", "expect zsticky 1000", "expect zstick -1000"]
    command(11, [8, 0, 0])
    lines += ["expect zsticky -1000", "wheel mod 64", "wheel pitch 0"]
    upload(wire)
    command(8, [1])
    lines += ["wait 150", "expect zksmode 2"]
    command(8, [0])
    lines += ["expect zksmode 0"]
    command(2, [*septets(len(wire), 2), *septets(crc, 3)])
    lines.extend(["usb unplug", "usb plug"])
    command(3, [0, 0, 0], status=2)
    command(2, [*septets(len(wire), 2), *septets(crc, 3)])
    lines.append("wait 2050")
    command(3, [0, 0, 0], status=2)
    legato = {**phrase, "sequence": {"lengthMs": 600, "events":
               [[0, 60, 100, True], [180, 67, 115, True], [410, 60, 100, False]]}}
    states = encode_patch(legato)
    upload(states)
    state_crc = binascii.crc_hqx(bytes(states), 0xffff)
    command(9, [1], answer=[*septets(len(states), 2), *septets(state_crc, 3), 0])
    for offset in range(0, len(states), 96):
        command(10, septets(offset, 2), answer=states[offset:offset + 96])
    upload(wire)
    command(11, [0, 1])
    lines += ["wait 5", "expect zattackheld 1", "wait 760", "expect zattackheld 0"]
    command(11, [1, 127, 127])
    lines.append("expect zstick 1000")
    command(11, [2, 1, *septets(148, 2), 50, 1, 5])
    command(11, [2, 1, *septets(148, 2), 50, 1, 8], status=1)
    command(11, [2, 1, *septets(148, 2), 50, 1])
    lines += ["expect zarp 1", "expect zarpbpm 148", "expect zarporder 1", "expect zarpdivision 5"]
    command(11, [3, 2, 31])
    command(11, [3, 2, 32], status=1)
    lines += ["expect zscaleroot 2", "expect zscale 31"]
    command(11, [2, 0, *septets(120, 2), 60, 0])
    command(11, [3, 0, 0])
    command(11, [5, 3, 41])
    lines += ["expect zhold 1", "expect zfree 1", "expect zchance 41"]
    command(11, [5, 4, 100], status=1)
    command(11, [5, 0, 101], status=1)
    command(11, [5, 0, 100])
    lines += ["master 0", "expect usb_audio_full 1", "usbwav studio-usb.wav", "wav studio-analog.wav"]
    command(8, [1])
    lines += ["wait 1100", "expect zmiss 0", "expect store_writes 0"]
    command(8, [0])
    lines += ["wavstop", "usbwavstop"]
    for index, entry in enumerate(catalog.values()):
        sequence = entry.patch.get("sequence")
        if not sequence:
            continue
        events = sequence["events"]
        steps = sum(event[2] > 0 for event in events)
        lines += [f"midi C0 {index:02X}", f"expect zseqlength {steps}", f"expect zseqvel {events[0][2]}"]
    effects = load(usb_effects=True)
    lines += ["expect zusbmatrix 0", "midi B0 00 0A", "midi C0 00", "expect zpreset 132",
              "expect zusbmatrix 1", "tap SEL", "tap RIGHT"]
    upload(encode_extended(effects["usb-stereo"].patch))

    def capture(key, channel_mask):
        lines.extend([f"usbsource {channel_mask}", "wait 80", f"usbwav {key}.wav",
                      "wait 180", "usbwavstop"])

    capture("matrix-usb-left", 1)
    capture("matrix-usb-right", 2)
    capture("matrix-usb-stereo", 3)
    lines += ["spin K4 -64", "wait 100"]
    capture("matrix-usb-gain-zero", 3)
    lines += ["spin K4 64", "wait 100"]
    for key in ("usb-filter", "usb-spring", "usb-ring"):
        upload(encode_extended(effects[key].patch))
        capture(f"matrix-{key}", 3)
    capture("matrix-usb-tail", 0)
    for row, col, _ in effects["usb-ring"].patch["pins"]:
        command(6, [row, col, 0])
    for channel, col, _ in effects["usb-ring"].patch["usbRoutes"]:
        command(14, [channel, col, 0])
    capture("matrix-usb-disconnected", 3)
    lines += ["expect zusbdrop 0", "expect zmiss 0", "tap LFO", "expect zusbmatrix 0", "usbsource 0"]
    # A regular keyboard patch must still sound with USB mode armed and no host samples.
    lines += ["midi B0 00 00", "midi C0 69", "tap LFO", "expect zusbmatrix 1",
              "master 4096", "peakreset", "key37 12 down 110", "wait 200", "expect peak_db_min -36",
              "key37 12 up", "wait 120", "tap LFO", "expect zusbmatrix 0"]
    # Preserve a detailed step grid and performance settings through the wire and flash.
    extended = encode_extended(phrase, note=67, velocity=96, length=7, bpm=156)
    upload(extended)
    lines += ["expect zseqlength 7", "expect zseqvel 96"]
    command(9, [2], answer=[*septets(len(extended), 2), *septets(binascii.crc_hqx(bytes(extended), 0xffff), 3), 0])
    for offset in range(0, len(extended), 96):
        command(10, septets(offset, 2), answer=extended[offset:offset + 96])
    fingerprint = [*septets(len(extended), 2), *septets(binascii.crc_hqx(bytes(extended), 0xffff), 3)]
    command(17, septets(0, 3), answer=[2, *septets(1, 3), *fingerprint, 0])
    for offset in range(0, len(extended), 96):
        command(10, septets(offset, 2), answer=extended[offset:offset + 96])
    command(17, septets(1, 3), answer=[0, *septets(1, 3)])
    command(9, [3, *fingerprint], answer=[0, 0, *fingerprint[2:], 0])
    command(6, [0, 1, 2])
    command(5, [5, *encode_float(1050)])
    for revision in (1, 1, 2):
        body = [0, *septets(2, 3)] if revision == 2 else [1, *septets(2, 3), 5, *encode_float(1050), 64, 0, 1, 2]
        command(17, septets(revision, 3), answer=body)
    command(5, [5, *encode_float(970)])
    changed = extended.copy()
    changed[len(phrase["controls"]) * 5 + 1] = 2
    command(9, [3, *fingerprint], answer=[*septets(len(changed), 2),
                                        *septets(binascii.crc_hqx(bytes(changed), 0xffff), 3), 0])
    for offset in range(0, len(changed), 96):
        command(10, septets(offset, 2), answer=changed[offset:offset + 96])
    command(6, [0, 1, extended[len(phrase["controls"]) * 5 + 1]])
    command(17, septets(2, 3), answer=[1, *septets(3, 3), 5, *encode_float(970), 64, 0, 1, 0])
    command(17, septets(1, 3), answer=[2, *septets(3, 3), *fingerprint, 0])
    for offset in range(0, len(extended), 96):
        command(10, septets(offset, 2), answer=extended[offset:offset + 96])
    command(11, [11, 0, *septets(16383, 2)])
    command(16, [16, *septets(2048, 2)])
    lines.append("expect zstick 500")
    command(16, [16, *septets(0, 2)])
    lines.append("expect zstick 0")
    command(16, [16, *septets(4096, 2)])
    lines.extend(["expect zstick 1000", "wheel pitch -8192", "expect zstick -1000", "wheel pitch 0"])
    command(18, [1], answer=[0] * 15 + septets(0xffffffff, 5) + [0] * 20)
    command(16, [0, *septets(2048, 2)])
    lines += ["expect zmaster 2048", "master 4096"]
    command(16, [0, *septets(4097, 2)], status=1)
    command(12, [0, 8, *septets(155, 2)])
    command(12, [1, 0, 69, 88])
    command(15, [0, 84])
    command(15, [24, 81], status=1)
    command(12, [2, 1])
    lines += ["expect zseqlength 8", "expect zseqvel 88", "expect zseqplaying 1"]
    command(13, [2, 0], status=5)
    command(12, [2, 0])
    command(13, [0], answer=[0, 0, 0])
    command(13, [2, 0])
    command(13, [0], answer=[1, 0, 0])
    command(12, [1, 0, 57, 12])
    lines.append("flash_fail 1")
    command(13, [2, 0], status=4)
    lines += ["flash_fail 0", "expect store_writes 1", "reboot", "wait 100"]
    command(13, [1, 0])
    lines += ["expect zseqlength 8", "expect zseqvel 88"]
    # Read and replace only the SEQ, leaving controls, pins and KS intact.
    updated = encode_extended(phrase, note=72, velocity=106, length=10, bpm=130)
    grid = updated[-(24 + 774):-24]
    grid[8] = 1
    grid[9:18] = [*septets(4200, 3), *septets(125, 3), *septets(1600, 3)]
    grid_crc = binascii.crc_hqx(bytes(grid), 0xffff)
    command(13, [3, 1], status=4)

    def upload_grid(value, status=0):
        command(2, [*septets(len(value), 2), *septets(binascii.crc_hqx(bytes(value), 0xffff), 3), 1])
        for offset in range(0, len(value), 96):
            command(3, [*septets(offset, 2), *value[offset:offset + 96]])
        command(4, status=status)

    command(12, [2, 1])
    upload_grid(grid, status=5)
    command(12, [2, 0])
    corrupt_grid = grid.copy()
    corrupt_grid[8] = 2
    upload_grid(corrupt_grid, status=1)
    lines += ["expect zseqlength 8", "expect zseqvel 88"]
    upload_grid(grid)
    lines += ["expect zseqlength 10", "expect zseqvel 106", "expect zcut 970", "expect zksevents 16"]
    command(12, [4], answer=[*septets(len(grid), 2), *septets(grid_crc, 3)])
    for offset in range(0, len(grid), 96):
        command(10, septets(offset, 2), answer=grid[offset:offset + 96])
    command(5, [5, *encode_float(2222)])
    command(13, [3, 0])
    lines += ["expect zcut 2222", "expect store_writes 2", "reboot", "wait 100"]
    command(13, [1, 0])
    lines += ["expect zcut 970", "expect zksevents 16", "expect zseqlength 10", "expect zseqvel 106"]
    command(13, [1, 1], status=4)
    lines += ["press GLO", "tap HOME", "release GLO", "expect zdaw 1",
              "spin K1 1", "expect_out USB B0 54 41"]
    command(16, [12, 1, 0])
    lines += ["spin K1 2", "expect_out USB B0 54 02", "spin K1 -2", "expect_out USB B0 54 7E"]
    script = output / "studio-midi.txt"
    script.write_text("\n".join(lines) + "\n")
    return script


def verify_sparse(simulator, output):
    signature = [0xf0, 0x7d, 0x5a, 0x53, 1]
    lines = ["wait 250"]
    serial = 0

    def command(op, payload, answer=(), status=0):
        nonlocal serial
        serial += 1
        head = [*signature, op, *septets(serial, 2)]
        lines.append("sysex " + bytes([*head, *payload, 0xf7]).hex(" "))
        head[5] |= 64
        lines.append("expect_sysex " + bytes([*head, status, *answer, 0xf7]).hex(" "))

    def reply(known, revision, kind, data):
        header = [kind, *septets(revision, 3), *septets(len(data), 2),
                  *septets(binascii.crc_hqx(bytes(data), 0xffff), 3)]
        if len(data) <= 87:
            command(19, septets(known, 3), [*header, *data])
        else:
            command(19, septets(known, 3), header)
            for offset in range(0, len(data), 96):
                command(19, septets(offset, 2), data[offset:offset + 96])

    def spans(old, new):
        wire = [127, 127]
        i = 0
        while i < len(new):
            if old[i] == new[i]:
                i += 1
                continue
            start = i
            while i < len(new) and old[i] != new[i] and i - start < 127:
                i += 1
            wire += [*septets(start, 2), i - start, *new[start:i]]
        return wire

    patch = load(felucca=True)["passos-ks"].patch
    wire = encode_extended(patch)
    parts = [*septets(len(wire), 2), *septets(binascii.crc_hqx(bytes(wire), 0xffff), 3)]
    command(2, parts)
    for offset in range(0, len(wire), 96):
        command(3, [*septets(offset, 2), *wire[offset:offset + 96]])
    command(4, [])

    panel = [3500, 3100, 2100, 0, 0, 1, 0, 0, 0, 120, 0, 0, 0, 0,
             8192, 8192, 4096, 4096, 8192, 8192, 1]
    for index in (*range(14), 16, 17, 20):
        command(16, [index, *septets(panel[index], 2)])
    command(11, [11, 1, *septets(8192, 2)])
    base = len(patch["controls"]) * 5 + 270
    ext = len(wire) - (32 + 9 + 6 + 64 * 12 + 24)
    original = [v for x in panel for v in septets(x, 2)] + wire[:base] + wire[ext:]
    phrase = wire[base:ext]
    reply(0, 1, 2, original + phrase)
    command(19, septets(1, 3), [0, *septets(1, 3)])

    current = original.copy()
    # A panel change alone uses one byte on the wire.
    command(16, [20, 0, 0])
    current[40] = 0
    reply(1, 2, 1, spans(original, current))
    command(19, septets(2, 3), [0, *septets(2, 3)])

    prior = current.copy()
    # Emulate a control change from the physical SMK as well as writes from the editor.
    lines.extend(["spin K8 -1", "expect zmaster <3500"])
    command(5, [5, *encode_float(1200)])
    command(6, [15, 9, 2])
    command(20, [0, 2, *septets(150, 2)])
    command(20, [1, 8, 7, 0, 9, 2, 0])
    command(20, [1, 8, 12, 0, 9, 3, 0], status=1)
    command(20, [2, 0, 7, 0])
    # Reestablish the known volume before comparing the patch-only wire regions.
    command(16, [0, *septets(panel[0], 2)])
    current[42 + 25:42 + 30] = encode_float(1200)
    matrix = 42 + len(patch["controls"]) * 5
    pos = matrix + 15 * 16 + 9
    current[pos] = current[pos] // 3 * 3 + 2
    current[matrix + 260:matrix + 262] = septets(150, 2)
    performance = 42 + base + 32
    current[performance + 7:performance + 9] = [7, 2]
    current[performance + 9] = 7
    reply(2, 3, 1, spans(prior, current))
    reply(2, 3, 1, spans(prior, current))
    reply(1, 3, 2, current + phrase)
    command(19, septets(3, 3), [0, *septets(3, 3)])
    script = output / "studio-sparse.txt"
    script.write_text("\n".join(lines) + "\n")
    subprocess.run([str(simulator.resolve()), script, output], check=True)


def verify_step_status(simulator, output):
    # Compatibility and clock transitions on the firmware's actual transport.
    common = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 120, 0, 60, 0, 0, 0, 0]
    lines = ["wait 250"]

    def query(serial, mode, answer):
        start = [0xf0, 0x7d, 0x5a, 0x53, 1]
        request = bytes([*start, 11, serial, 0, mode, 0xf7])
        reply = bytes([*start, 75, serial, 0, 0, *answer, 0xf7])
        lines.extend(("sysex " + request.hex(" "), "expect_sysex " + reply.hex(" ")))

    query(1, 4, common)
    query(2, 6, [*common, 0, 100])
    query(3, 7, [*common, 0, 100, 0, 0, *septets(120, 2)])
    lines.extend(("tap SEL", "tap RIGHT", "spin PRESET 1", "tap SEL", "tap SEQ",
                  "midi FA", "midi F8", "wait 18", "midi F8"))
    query(4, 7, [*common, 8, 100, 0, 3, *septets(125, 2)])
    lines.extend(("midi FC", "wait 520"))
    query(5, 7, [*common, 0, 100, 0, 1, *septets(120, 2)])

    script = output / "studio-step-clock.txt"
    script.write_text("\n".join(lines) + "\n")
    subprocess.run([str(simulator.resolve()), script, output], check=True)


def peak(path):
    with wave.open(str(path)) as wav:
        assert wav.getframerate() == 44100 and wav.getnchannels() == 2
        return max(abs(v) for (v,) in struct.iter_unpack("<h", wav.readframes(wav.getnframes())))


def channel_rms(path):
    with wave.open(str(path)) as wav:
        assert wav.getframerate() == 44100 and wav.getnchannels() == 2 and wav.getsampwidth() == 2
        frames = wav.getnframes()
        left = right = 0
        for l, r in struct.iter_unpack("<hh", wav.readframes(frames)):
            left += l * l
            right += r * r
        return math.sqrt(left / frames), math.sqrt(right / frames)


def verify_filter_alignment(simulator, output):
    controls = load(felucca=True)["passos-ks"].patch["controls"]
    wire = encode_extended({"controls": {**controls, "cutoff": 150.855, "response": 1},
                            "pins": [[9, 0, 1], [9, 2, 1]], "sequenceTrigger": False})
    serial = 0
    lines = ["wait 250"]

    def send(command, data=()):
        nonlocal serial
        serial += 1
        frame = bytes([0xf0, 0x7d, 0x5a, 0x53, 1, command, *septets(serial, 2), *data, 0xf7])
        reply = bytes([0xf0, 0x7d, 0x5a, 0x53, 1, command | 64, *septets(serial, 2), 0, 0xf7])
        lines.extend(("sysex " + frame.hex(" "), "expect_sysex " + reply.hex(" ")))

    send(2, [*septets(len(wire), 2), *septets(binascii.crc_hqx(bytes(wire), 0xffff), 3)])
    for offset in range(0, len(wire), 96):
        send(3, [*septets(offset, 2), *wire[offset:offset + 96]])
    send(4)
    lines += ["key37 12 down 110", "wait 650", "usbwav filter-alignment.wav",
              "wait 1100", "usbwavstop", "key37 12 up"]
    script = output / "filter-alignment.txt"
    script.write_text("\n".join(lines) + "\n")
    subprocess.run([str(simulator.resolve()), script, output], check=True)
    with wave.open(str(output / "filter-alignment.wav")) as wav:
        rate = wav.getframerate()
        previous = 0
        crossings = []
        for frame, (value, _) in enumerate(struct.iter_unpack("<hh", wav.readframes(wav.getnframes()))):
            if frame > rate // 5 and value > 0 >= previous:
                crossings.append(frame - 1 - previous / (value - previous))
            previous = value
    assert len(crossings) > 200, len(crossings)
    hz = rate * (len(crossings) - 1) / (crossings[-1] - crossings[0])
    assert abs(hz - 261) < 0.8, hz
    print(f"Auto-oscilação no firmware Felucca em host, vernier do filtro na posição 5: {hz:.2f} Hz.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("simulator", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(args.simulator.resolve()), script_for(args.output), args.output], check=True)
    verify_step_status(args.simulator, args.output)
    verify_sparse(args.simulator, args.output)
    verify_filter_alignment(args.simulator, args.output)
    usb_peak = peak(args.output / "studio-usb.wav")
    assert 500 < usb_peak <= 29206
    assert peak(args.output / "studio-analog.wav") == 0
    print(f"USB com K8 em zero: {20 * math.log10(usb_peak / 32768):.2f} dBFS; saída analógica digitalizada em silêncio.")
    capture = {key: channel_rms(args.output / f"matrix-usb-{key}.wav") for key in
               ("left", "right", "stereo", "gain-zero", "filter", "spring", "ring", "tail", "disconnected")}
    l, r = capture["stereo"]
    assert 600 < l < 20000 and 300 < r < l, capture
    # DC blocking from a preceding patch or capture can leave a sub-2 LSB tail.
    assert capture["left"][0] > 600 and capture["left"][1] < 2, capture
    assert capture["right"][0] < 2 and capture["right"][1] > 300, capture
    for key in ("gain-zero", "tail", "disconnected"):
        assert max(capture[key]) < 2, capture
    filter_l, filter_r = capture["filter"]
    ring_l, ring_r = capture["ring"]
    assert min(filter_l, filter_r, ring_l, ring_r) > 40, capture
    assert abs(filter_l - filter_r) < 1 and abs(ring_l - ring_r) < 1 and abs(filter_l - ring_l) > 40, capture
    assert abs(capture["spring"][0] - l) > 10, capture
    print("Matriz USB: canais isolados, ganho USB em zero, filtro, mola, anel e matriz desconectada verificados.")


if __name__ == "__main__":
    main()
