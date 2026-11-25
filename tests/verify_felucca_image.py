"""Inspect the Pro package; optionally compare the Open update's VM boundary."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
from zipfile import ZipFile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("build", type=Path)
parser.add_argument("fm1_base", type=Path)
parser.add_argument("--open-zip", type=Path)
parser.add_argument("--previous-zip", type=Path)
args = parser.parse_args()
sys.path.insert(0, str(args.fm1_base.resolve() / "tools"))
from fm1pkg_make import BOARDS, KEY, crc16, enc, sfc


def contents(raw):
    identity = "".join(chr((raw[i * 48 + 47] - i - 1) & 255)
                       for i in range(20) if raw[i * 48 + 47] != 0x7D)
    logical = b"".join(raw[i:i + 47] for i in range(0, 960, 48)) + raw[960:]
    header = bytearray(logical[:64])
    enc(header, 0, 64)
    expected, table_crc, size, count = struct.unpack_from("<HHIH", header)
    assert expected == crc16(header[2:]) and size == len(logical)
    assert table_crc == crc16(logical[64:64 + count * 80])
    files = {}
    for i in range(count):
        entry = bytearray(logical[64 + i * 80:144 + i * 80])
        enc(entry, 0, 80)
        _, _, crc, _, offset, size = struct.unpack_from("<HHHHII", entry)
        payload = logical[offset:offset + size]
        assert len(payload) == size and crc16(payload) == crc
        files[bytes(entry[64:80]).split(b"\0", 1)[0].rstrip(b"\xff")] = payload
    return identity, files


def records(flash):
    area = bytearray(flash[0x4000:])
    sfc(area, 0, len(area), 0, KEY)
    out = {}
    for off in range(0, 0x120, 32):
        entry = area[off:off + 32]
        expected, data_crc, addr, size = struct.unpack_from("<HHII", entry)
        if expected != crc16(entry[2:]):
            break
        name = bytes(entry[16:32]).split(b"\0", 1)[0]
        out[name] = (data_crc, addr, size)
    return out, area


def budgets(link_map, app_size, update_size):
    def span(start, end):
        def addr(symbol):
            match = re.search(rf"^\s*0x([0-9a-f]+)\s+{symbol}\s+= \.", link_map, re.M)
            assert match, symbol
            return int(match[1], 16)
        return addr(end) - addr(start)

    return {
        "Aplicação (flash)": (app_size, BOARDS["smk37"]["app_slot"]),
        "RAM para código": (span("_rt_start", "_rt_end"), 0x8000),
        "RAM estática": (span("_data_start", "_bss_end"), 96 * 1024),
        "Pool de buffers": (span("_pool_start", "_pool_end"), 0x54000),
        "Pacote FWSC": (update_size, None),
    }

build = args.build
report = json.loads((build / "build.json").read_text())
for name, expected in report["sha256"].items():
    assert hashlib.sha256((build / name).read_bytes()).hexdigest() == expected
identity, files = contents((build / "zivisynthi-felucca-smk37.fwsc").read_bytes())
assert identity == report["identity"] == "SMK-37 Pro_99400"
assert len(files[b"ota.bin"]) < 0x4F00
flash = files[b"flash.bin"]
assert len(flash) == 0x9D000
entries, area = records(flash)
assert entries[b"VM"][1:] == (0x9D000, 0x23000)
assert entries[b"BTIF"][1:] == (0xC0000, 0x1000)
assert entries[b"USRTRIM"][1:] == (0xC1000, 0x1000)
assert entries[b"USR"][1:] == (0xF4000, 0xA000)
checksum, start, size = entries[b"app.bin"]
image = area[start:start + size]
app = (build / "zivisynthi-felucca.bin").read_bytes()
assert image[:len(app)] == app and set(image[len(app):]) == {255}
assert crc16(image) == checksum
print(f"{identity}: UFW, CRC, loader, app.bin e limites de flash verificados.")
current = budgets((build / "zivisynthi-felucca.map").read_text(), len(app),
                  (build / "zivisynthi-felucca-smk37.fwsc").stat().st_size)
previous = None
if args.previous_zip:
    with ZipFile(args.previous_zip) as archive:
        base = "engenharia/zivisynthi-felucca"
        previous = budgets(archive.read(base + ".map").decode(),
                           archive.getinfo(base + ".bin").file_size,
                           archive.getinfo(base + "-smk37.fwsc").file_size)
print("Valores em bytes. As margens de RAM pertencem a regiões distintas.")
for name, (used, limit) in current.items():
    assert limit is None or used <= limit
    capacity = f" / {limit}, livres {limit - used}" if limit is not None else ""
    delta = f", diferença {used - previous[name][0]:+}" if previous else ""
    print(f"{name}: {used}{capacity}{delta}.")
if args.open_zip:
    with ZipFile(args.open_zip) as archive:
        name = next(name for name in archive.namelist() if "/" not in name and name.endswith(".fwsc"))
        _, old_files = contents(archive.read(name))
    old, _ = records(old_files[b"flash.bin"])
    assert old[b"VM"][1] == len(old_files[b"flash.bin"]) == 0x72000
    assert old[b"VM"][1] + old[b"VM"][2] <= entries[b"VM"][1]
    print(f"VM da versão Open está dentro da área de aplicação Felucca (0x{old[b'VM'][1]:05X} < 0x{len(flash):05X}).")
