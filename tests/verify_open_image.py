import binascii
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

folder = Path(sys.argv[1])
fw = next(folder.glob("*Open-0.8.2-alpha2*.fwsc")).read_bytes()
elf = next(folder.glob("*Open-0.8.2-alpha2*.elf"))
tool = sys.argv[2]
ufw = b"".join(fw[i:i + 47] for i in range(0, 960, 48)) + fw[960:]
crc = lambda data: binascii.crc_hqx(data, 0)

def decode(data, seed):
    seed %= 65536
    result = []
    for byte in data:
        result.append(byte ^ seed % 256)
        seed = ((seed * 2) ^ (4129 * (seed >= 32768))) % 65536
    return bytes(result)

header = decode(ufw[64:144], 65535)
offset, amount = struct.unpack_from("<II", header, 8)
image = ufw[offset:offset + amount]
assert crc(image) == int.from_bytes(header[4:6], "little")
base = 16384
decoded = b"".join(decode(image[base + at:base + at + 32], 0x980f ^ (at // 4))
                   for at in range(0, len(image) - base, 32))

def parse(at, expected):
    record = decoded[at:at + 32]
    assert record[16:32].split(b"\0")[0] == expected
    hcrc, dcrc, address, size = struct.unpack_from("<HHII", record)
    assert crc(record[2:]) == hcrc
    return dcrc, address, size

d, address, size = parse(0, b"app_area_head")
assert address == 0x2000120 and crc(decoded[32:size]) == d
_, addr_vm, vm = parse(96, b"VM")
_, addr_bt, bt = parse(160, b"BTIF")
_, addr_user, user = parse(size + 32, b"USER")
assert addr_vm == len(image) == 466944
assert vm >= 32768 and addr_vm + vm == addr_bt and addr_bt + bt == addr_user
assert bt == 4096 and user == 536576 and addr_user + user == 0xff000
d, address, length = parse(32, b"app.bin")
app_image = decoded[address:address + length]
assert crc(app_image) == d
with tempfile.TemporaryDirectory() as temp:
    joined = b""
    for section in (".text", ".data", ".ram0_data", ".cache_ram_data"):
        out = Path(temp)/section
        subprocess.run([tool, "-O", "binary", "-j", section, elf, out], check=True)
        joined += out.read_bytes()
assert app_image == joined
print("FWSC inner flash directory, VM/BTIF/USER boundaries, app CRC and byte identity with the four ELF sections validated.")
