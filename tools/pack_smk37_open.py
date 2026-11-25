"""Python translation of SMK37 Open sdk/Package.cs for the Linux build.

SPDX-License-Identifier: MIT
Copyright (c) 2023 Andrey Grigoryev (kagaimiq), 2026 M-VAVE.
See the SDK's THIRD_PARTY.md for notices and the license.
"""
import binascii
from pathlib import Path
import re
import struct

AREA = 0x4000
ENTRY = 0x02000120
APP_DATA = 0x120
FLASH_LIMIT = 0xff000
USER_SIZE = 536576
BT_SIZE = 4096
MIN_VM = 32768
CODE = 0x980f

HEADER = bytes.fromhex(
    "000020022f104cc900f00f001010a4ff4e5d0b41e0cd781dc856417fb367cf9f"
)


def crc(data):
    return binascii.crc_hqx(data, 0)


def encrypt(data, key=0xffff):
    output = bytearray(data)
    for i, value in enumerate(data):
        output[i] = value ^ (key & 0xff)
        key = ((key << 1) ^ (0x1021 if key & 0x8000 else 0)) & 0xffff
    return bytes(output)


def checksum(data, field, begin, size):
    struct.pack_into("<H", data, field, crc(data[begin:begin + size]))


def round_up(value, alignment):
    return (value + alignment - 1) & -alignment


def record(name, data_crc, address, size, attributes, last=0, extra=None):
    data = bytearray(b"\0" * 16 + b"\xff" * 16)
    label = name.encode("ascii") + b"\0"
    data[16:16 + len(label)] = label
    struct.pack_into("<HHIIHH", data, 0, 0, data_crc, address, size, attributes, last)
    if extra is not None:
        struct.pack_into("<I", data, 28, extra)
    checksum(data, 0, 2, 30)
    return data


def resource(folder, name, expected):
    data = (folder / name).read_bytes()
    if len(data) != expected:
        raise ValueError(f"Invalid SMK37 Open resource {name}.")
    return data


def encode_boot(plain):
    banks, = struct.unpack_from("<H", plain)
    if not 1 <= banks <= len(plain) // 16:
        raise ValueError("Invalid boot bank count.")
    previous_end = banks * 16
    output = bytearray(plain)
    for i in range(banks):
        offset = i * 16
        _, length, _, start, data_crc, head_crc = struct.unpack_from("<HHIIHH", plain, offset)
        if (not length or not previous_end <= start <= len(plain) - length
                or head_crc != crc(plain[offset:offset + 14])
                or data_crc != crc(plain[start:start + length])):
            raise ValueError("Invalid boot bank.")
        output[offset:offset + 16] = encrypt(plain[offset:offset + 16])
        output[start:start + length] = encrypt(plain[start:start + length])
        previous_end = start + length
    return output


def create_flash(app, folder):
    boot = resource(folder, "uboot.boot", 14488)
    config = resource(folder, "boot-config.bin", 721)
    tool = resource(folder, "cfg_tool.bin", 383)
    user_start = FLASH_LIMIT - USER_SIZE
    vm_end = user_start - BT_SIZE
    payload = APP_DATA + len(app) + len(tool)
    end = round_up(AREA + payload + 64, 4096)
    if not app or end > vm_end - MIN_VM:
        raise ValueError(f"APP with {len(app)} bytes exceeds SMK37 Open layout.")
    settings = bytearray(34) + config
    for bit in range(16):
        settings[bit] = 16
        settings[bit + 16] = 16 ^ (0 if CODE & (1 << bit) else 255)
    checksum(settings, 32, 0, 32)

    flash = bytearray(b"\xff" * end)
    flash[:32] = HEADER
    checksum(flash, 0, 2, 30)
    config_at = 160 + len(boot)
    flash[32:64] = record("uboot.boot", crc(boot), 160, len(boot), 0)
    flash[64:96] = record("isd_config.ini", crc(settings), config_at, len(settings), 0x8002)
    flash[96:128] = record("app_dir_head", 0xffff, AREA, 0xffffffff, 0xff81)
    flash[128:160] = record("key_mac", 0xffff, FLASH_LIMIT, 4096, 0x0112, 0xffff)
    for at in range(0, 160, 32):
        flash[at:at + 32] = encrypt(flash[at:at + 32])
    flash[160:config_at] = encode_boot(boot)
    flash[config_at:config_at + len(settings)] = settings

    flash[AREA + APP_DATA:AREA + payload] = app + tool
    entries = [
        record("app.bin", crc(app), APP_DATA, len(app), 0xff82),
        record("cfg_tool.bin", crc(tool), APP_DATA + len(app), len(tool), 0xff82),
        record("VM", 0xffff, end, vm_end - end, 0x8112, extra=MIN_VM),
        record("PRCT", 0xffff, 0, end, 0x8292, extra=0x45444f43),
        record("BTIF", 0xffff, vm_end, BT_SIZE, 0x8192, 1, 0x4f545541),
    ]
    flash[AREA + 32:AREA + 192] = b"".join(entries)
    flash[AREA:AREA + 32] = record("app_area_head", crc(flash[AREA + 32:AREA + payload]),
                                   ENTRY, payload, 0xff83)
    user = record("USER", 0xffff, user_start, USER_SIZE, 0x8192, 1, 0x4f545541)
    flash[AREA + payload:AREA + payload + 64] = (
        record("EXT_RESERVED", crc(user), 32, 64, 0xff93, 1) + user
    )
    for offset in range(0, payload + 64, 32):
        at = AREA + offset
        last = AREA + min(offset + 32, payload + 64)
        flash[at:last] = encrypt(flash[at:last], CODE ^ (offset >> 2))
    return flash, dict(flash_bytes=end, vm_bytes=vm_end - end, user_bytes=USER_SIZE,
                       btif_bytes=BT_SIZE)


def wrap(flash, loader, product):
    if not re.fullmatch(r"SMK-37 Pro_8[0-9]{6}", product) or not 0 < len(loader) < 32768:
        raise ValueError("Invalid Pro identity or update loader.")
    loader_at = 1024 + round_up(len(flash), 32)
    total = loader_at + round_up(len(loader), 32)
    ufw = bytearray(b"\xff" * total)
    ufw[:224] = bytes(224)
    for i, (payload, name, kind, start) in enumerate(
        ((flash, b"flash.bin", 0, 1024), (loader, b"ota.bin", 100, loader_at))
    ):
        at = 64 + i * 80
        struct.pack_into("<HHH2xIII", ufw, at, kind, i, crc(payload), start, len(payload),
                         round_up(len(payload), 32))
        ufw[at + 64:at + 64 + len(name)] = name
        ufw[at:at + 80] = encrypt(ufw[at:at + 80])
        ufw[start:start + len(payload)] = payload
    struct.pack_into("<IHHI", ufw, 4, total, 2, 4, 512)
    ufw[16:22] = b"AC791N"
    checksum(ufw, 2, 64, 160)
    checksum(ufw, 0, 2, 62)
    ufw[:64] = encrypt(ufw[:64])
    blocks = [ufw[i * 47:(i + 1) * 47] +
              bytes([ord(product[i]) + i + 1 if i < len(product) else 125]) for i in range(20)]
    return b"".join(blocks) + ufw[940:]


def package(app, folder: Path, product):
    flash, layout = create_flash(app, folder)
    return wrap(flash, (folder / "ota.bin").read_bytes(), product), layout
