"""Audit the linked SMK USB descriptor graph, including MIDI and both PCM streams."""
import re


def linked_usb_config(image, symbols, base):
    match = re.search(r"(?m)^([\da-f]+)\s+.*?\.text\s+([\da-f]+)\s+CFG_DESC$", symbols)
    if not match:
        raise ValueError("Linked USB configuration descriptor is missing")
    start, length = (int(value, 16) for value in match.groups())
    offset = start - base
    config = image[offset:offset + length]
    if len(config) != length or config[:2] != b"\x09\x02" or int.from_bytes(config[2:4], "little") != length:
        raise ValueError("Invalid USB configuration length")
    return config


def check(image, symbols, base):
    config = linked_usb_config(image, symbols, base)
    interfaces = {}
    current = None
    pos = 9
    while pos < len(config):
        length = config[pos]
        if length < 2 or pos + length > len(config):
            raise ValueError("USB descriptor crosses its configuration boundary")
        block = config[pos:pos + length]
        if block[1] == 4:
            current = (block[2], block[3])
            if current in interfaces:
                raise ValueError(f"Duplicate USB interface {current}")
            interfaces[current] = dict(header=block, blocks=[])
        else:
            interfaces[current]["blocks"].append(block)
        pos += length

    if config[4] != 4 or set(interfaces) != {(0, 0), (1, 0), (2, 0), (2, 1), (3, 0), (3, 1)}:
        raise ValueError("Wrong interface/alternate-setting set")
    ac = interfaces[(0, 0)]["blocks"]
    if (ac[0][7:] != bytes([3, 2, 3, 1]) or
            int.from_bytes(ac[0][5:7], "little") != sum(len(entry) for entry in ac)):
        raise ValueError("Invalid AudioControl collection")
    for key, endpoints in {
        (0, 0): [], (1, 0): [0x01, 0x81], (2, 0): [], (2, 1): [0x84], (3, 0): [], (3, 1): [0x03],
    }.items():
        entry = interfaces[key]
        blocks = [block for block in entry["blocks"] if block[1] == 5]
        if entry["header"][4] != len(endpoints) or [block[2] for block in blocks] != endpoints:
            raise ValueError(f"Wrong USB endpoints on interface {key}")
        if key[0] >= 2 and key[1] == 1:
            endpoint = blocks[0]
            fmt = next(block for block in entry["blocks"] if block[1:3] == b"\x24\x02")
            rates = [int.from_bytes(fmt[i:i+3], "little") for i in range(8, len(fmt), 3)]
            wanted = [44100, 48000] if key[0] == 2 else [44100]
            if fmt[4:7] != bytes([2, 2, 16]) or fmt[7] != len(wanted) or rates != wanted:
                raise ValueError(f"Unsupported audio format on interface {key}")
            if endpoint[3] != (5 if key[0] == 2 else 9) or endpoint[6] != 1:
                raise ValueError(f"Invalid isochronous endpoint on interface {key}")
            if int.from_bytes(endpoint[4:6], "little") < max(rates) // 1000 * 4 + 4:
                raise ValueError(f"Audio endpoint packet too small on interface {key}")
    return "USB MIDI + PCM stereo signed 16 bits: capture 44.1/48 kHz, playback 44.1 kHz"
