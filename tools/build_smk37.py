#!/usr/bin/env python3
"""Build and inspect the ziviSynthiA SMK-37 Pro application and FWSC on Linux."""
import argparse
import hashlib
import importlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

import check_smk_usb

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "build" / "smk37-firmware"
PRODUCT = "SMK-37 Pro_971"
VERSION = "0.7.1-alpha1"
STEM = "ziviSynthiA-SMK37-Pro-" + VERSION


def run(*cmd, env=None):
    subprocess.run([str(arg) for arg in cmd], cwd=ROOT, env=env, check=True)


def prepare_dependencies(bsp, fonts):
    # Only files used by this target or the update-loader build are staged.
    for relative in ("firmware/hal", "firmware/loader"):
        shutil.copytree(bsp / relative, OUTPUT / relative, dirs_exist_ok=True)
    for relative in ("firmware/crt0.S", "firmware/app.ld", "firmware/src/ota.c",
                     "firmware/src/libc.c", "firmware/src/usb.c", "firmware/src/lcd.c",
                     "firmware/src/bootguard.h", "firmware/src/uac_fir.h", "firmware/src/smk_padlight.h",
                     "firmware/src/smk_wheel.h", "firmware/src/smk_midi_out.h",
                     "tools/build.py", "tools/lz4blk.py", "tools/fm1pkg_make.py",
                     "tools/fm1_install.py"):
        target = OUTPUT / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(bsp / relative, target)
    env = {**os.environ, "X0X_FONTSET": "barlow",
           "FM1_FONT_TRIM": "0", "FM1_FONT_PACK": "0"}
    run(sys.executable, fonts / "tools" / "gen_font.py", OUTPUT / "zivi_fonts.h", env=env)
    usb = OUTPUT / "firmware/src/usb.c"
    source = usb.read_text()
    identity = re.compile(r"(?m)^static const uint8_t STR2\[\] = \{16, 3, 'F'.*?\};$")
    name = "ziviSynthiA SMK"
    descriptor = ", ".join(str(value) for value in bytes([2 + 2 * len(name), 3]) + name.encode("utf-16-le"))
    source, count = identity.subn(f"static const uint8_t STR2[] = {{{descriptor}}};", source)
    if count != 1:
        raise SystemExit("USB descriptor differs from SLOOP 2.8.5.")
    # A separate copy is used for the application so the OTA loader keeps its
    # upstream MIDI-only transport and the same recovery behavior.
    (usb.parent / "usb_zivi.c").write_text(add_usb_playback(source))


def add_usb_playback(source):
    def change(before, after, count=1):
        nonlocal source
        if source.count(before) != count:
            raise SystemExit(f"SLOOP USB driver changed at {before[:75]!r}")
        source = source.replace(before, after)

    change("101 + 74 * FELUCCA_CDC + 74 * FELUCCA_UAC",
           "101 + 74 * FELUCCA_CDC + (74 + 74) * FELUCCA_UAC")
    change("(2 + FELUCCA_UAC + 2 * FELUCCA_CDC)", "(2 + 2 * FELUCCA_UAC + 2 * FELUCCA_CDC)")
    change("10, 0x24, 1, 0x00, 0x01, 31, 0, 2, UAC_AS_IF, 1,",
           "11, 0x24, 1, 0x00, 0x01, 53, 0, 3, UAC_AS_IF, 3, 1,")
    change("9, 0x24, 3, 2, 0x01, 0x01, 0, 1, 0,",
           """12, 0x24, 2, 3, 0x01, 0x01, 0, 2, 3, 0, 0, 0,
    9, 0x24, 3, 4, 0x02, 0x03, 0, 3, 0,
    9, 0x24, 3, 2, 0x01, 0x01, 0, 1, 0,""")
    change("7, 0x25, 1, 0x01, 0, 0, 0,",
           """7, 0x25, 1, 0x01, 0, 0, 0,
    9, 4, 3, 0, 0, 1, 2, 0, 0,
    9, 4, 3, 1, 1, 1, 2, 0, 0,
    7, 0x24, 1, 3, 1, 1, 0,
    11, 0x24, 2, 1, 2, 2, 16, 1, 0x44, 0xac, 0,
    9, 5, 0x03, 0x09, 184, 0, 1, 0, 0,
    7, 0x25, 1, 0, 0, 0, 0,""")
    change("static void ep0_service(", '#include "usb_playback.h"\n\nstatic void ep0_service(')
    change("case 0x010B:", """case 0x010B:
        if (s[4] == PLAYBACK_IF && wvalue <= 1 && usb.config) {
            playback_stream(wvalue);
            goto ack;
        }""")
    change("case 0x810A:", """case 0x810A:
        if (s[4] == PLAYBACK_IF) {
            static uint8_t current;
            current = playback_alt;
            e0_send(&current, 1, wlength);
            return;
        }""")
    change("usb.config = s[2];", "usb.config = s[2];\n        playback_stream(0);")
    change("usb.config = 0;\n        usb.suspended", "usb.config = 0;\n        playback_stream(0);\n        usb.suspended")
    change("    uac_service();", """    if (usb.suspended && (playback_pcm.read != playback_pcm.write || playback_pcm.primed))
        pcm_reset(&playback_pcm);
    else if (ir & (1u << PLAYBACK_EP))
        playback_receive();
    uac_service();""")
    # Enable the EP3 receive interrupt status latch alongside MIDI EP1.
    change("sie_wr(S_INTRRX1E, 0x02);", "sie_wr(S_INTRRX1E, 0x0a);")
    change("if (cin == 0xFu && (st == 0xF8u", """if (cin == 3u && st == 0xf2 && !(pkt & 0x80800000u)) {
        if (mi_w - mi_r < MQ) {
            midi_in_q[mi_w % MQ] = pkt;
            RING_PUBLISH();
            mi_w++;
        }
        return;
    }
    if (cin == 0xFu && (st == 0xF8u""")
    change("""static void usb_start(void)                             /* boot, or main-loop retry while usb.up == 0 */
{
    usb.timeouts = 0;
    fm1_usb_reset();                                    /* reset whatever the ROM left */
    fm1_delay_ms(25);
""", """static void usb_start_finish(void)
{
""")
    change("/* Powered on without a cable,", """static void usb_start_begin(void)
{
    usb.timeouts = 0;
    fm1_usb_reset();
}

static void usb_start(void)
{
    usb_start_begin();
    fm1_delay_ms(25);
    usb_start_finish();
}

/* Powered on without a cable,""")
    return source


def build_image(bsp_build, toolchain, sdk):
    tc = bsp_build.tc
    memory_map = OUTPUT / "firmware/app.ld"
    script = memory_map.read_text()
    old_length = "LENGTH = 0x8DFBC"
    if script.count(old_length) != 1:
        raise SystemExit("Unexpected XIP map in the SMK dependency.")
    linker = OUTPUT / "app.ld"
    linker.write_text(script.replace(old_length, f"LENGTH = {bsp_build.APP_SLOT:#x}"))

    flags = ["-mcpu=r3", "-integrated-as", "-std=c11", "-ffreestanding",
             "-O2", "-ffp-contract=off", "-ffunction-sections", "-fdata-sections",
             "-DBOARD_SMK37=1", "-DZIVI_SMK_NATIVE", "-DZIVI_REAL32", "-DZIVI_MAX_RATE=44100",
             "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
             "-Wno-unused-variable", "-Wno-unused-parameter",
             f'-DFELUCCA_ID="{PRODUCT}"', f'-DZIVI_VERSION="{VERSION}"', "-isystem", str(toolchain / "pi32v2/include")]
    flags += ["-I" + str(path) for path in
              (ROOT / "app/core", ROOT / "platform/smk37_pro", OUTPUT,
               OUTPUT / "firmware/hal", OUTPUT / "firmware/src")]
    objects = []
    for relative in ("firmware/crt0.S", "firmware/hal/fm1_vec.S",
                     "firmware/hal/fm1_isr.S", "firmware/hal/fm1_cpu1.S"):
        src = OUTPUT / relative
        obj = OUTPUT / src.with_suffix(".o").name
        tc("cc", "-mcpu=r3", "-integrated-as", "-c", src, "-o", obj)
        objects.append(obj)
    for relative in ("app/core/synth.c", "app/core/modules.c", "app/core/ks.c",
                     "app/core/presets.c", "platform/smk37_pro/zivi_smk37.c",
                     "platform/smk37_pro/device.c"):
        src = ROOT / relative
        obj = OUTPUT / src.with_suffix(".o").name
        fp = ["-cl-single-precision-constant"] if src.parent == ROOT / "app/core" else []
        tc("cc", *flags, *fp, "-c", str(src), "-o", obj)
        objects.append(obj)

    for name in ("synth", "modules"):
        unresolved = tc("pi32v2/bin/nm", "-u", OUTPUT / (name + ".o"))
        if re.search(r"\b__(?:add|sub|mul|div)df3\b", unresolved):
            raise SystemExit(f"Software double arithmetic leaked into the board DSP ({name}).")

    elf = OUTPUT / "zivisynthi.elf"
    libs = sdk / "include_lib/newlib/pi32v2-lib"
    tc("pi32v2/bin/ld", "--gc-sections", "-e", "_start", "-T", linker,
       "-Map", OUTPUT / "zivisynthi.map", *objects, "--start-group",
       *(str(libs / lib) for lib in ("libm.a", "libc.a", "libcompiler_rt.a")),
       "--end-group", "-o", elf)
    syms = tc("common/bin/objdump", "-t", elf)
    dis = tc("common/bin/objdump", "-d", elf)
    ram = tc("common/bin/objdump", "-d", "-j", ".ram_text", elf)
    (OUTPUT / "zivisynthi.dis").write_text(dis)
    (OUTPUT / "symbols.txt").write_text(syms)
    image = bytearray()
    for name, symbol in ((".text", "_start"), (".ram_text", "_rt_load"),
                         (".c1_text", "_c1_load"), (".data", "_data_load")):
        part = OUTPUT / (name[1:] + ".bin")
        part.unlink(missing_ok=True)
        tc("common/bin/objcopy", "-O", "binary", "-j", name, elf, part)
        if not part.exists() or not part.stat().st_size:
            continue
        address = int(re.search(r"^([0-9a-f]+) .*\s" + symbol + "$", syms, re.M)[1], 16)
        offset = address - bsp_build.APP_XIP
        if offset < len(image) or offset % 4:
            raise SystemExit(f"Invalid {name} load address: {address:#x}")
        image.extend(b"\xff" * (offset - len(image)))
        image.extend(part.read_bytes())
    image.extend(b"\xff" * (-len(image) % 4))
    errors, notes = bsp_build.check(image, syms, dis, ram)
    notes.append(check_smk_usb.check(image, syms, bsp_build.APP_XIP))
    if errors:
        raise SystemExit("\n".join(errors))
    if not re.search(r"(?m)^01c0\w+ .*\.ram_text\s+\w+ zivi_smk_irq_read$", syms):
        raise SystemExit("Audio IRQ copy is outside the allowed RAM code section.")
    if tc("pi32v2/bin/nm", "-u", elf).strip():
        raise SystemExit("Unresolved symbols in the final ELF.")
    (OUTPUT / "zivisynthi.bin").write_bytes(image)
    return bytes(image), notes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("sloop", "fm1-base", "toolchain", "sdk"):
        parser.add_argument("--" + name, type=lambda p: Path(p).resolve(), required=True)
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    prepare_dependencies(args.sloop, args.fm1_base)
    sys.path.insert(0, str(OUTPUT / "tools"))
    bsp = importlib.import_module("build")
    packer = bsp.fm1pkg_make
    os.environ["JIELI_TOOLCHAIN"] = str(args.toolchain)
    packer.set_board("smk37")
    packer.SDK = args.sdk
    bsp.BOARD = "smk37"
    bsp.APP_SLOT = packer.APP_SLOT
    for name, expected in bsp.SDK_SHA256.items():
        if hashlib.sha256(packer.sdk_file(name)).hexdigest() != expected:
            raise SystemExit(f"Unexpected SHA-256 for SDK dependency {name}.")
    image, notes = build_image(bsp, args.toolchain, args.sdk)
    ota = bsp.build_loader()
    path = OUTPUT / (STEM + ".fwsc")
    path.write_bytes(packer.ufw(packer.flash_image(image, packer.KEY), ota, PRODUCT))
    installer = importlib.import_module("fm1_install")
    identity, decoded = installer.load_package(path, False)
    if identity != PRODUCT or installer.package_board(decoded) != "smk":
        raise SystemExit("The installer did not recognize this package as the SMK-37 Pro.")
    entries = installer.package_files(decoded)
    if [entry[0] for entry in entries] != [0, 100]:
        raise SystemExit(f"Unexpected payload entries: {entries}")
    _, flash_at, flash_size, _, _ = entries[0]
    flash = decoded[flash_at:flash_at + flash_size]
    if flash_size != 0x9d000 or installer.region_table(flash).get("USRTRIM") != (0xc1000, 0x1000):
        raise SystemExit("The flash payload or the calibration region differs from the Pro map.")
    if installer._sfc(flash[0x4120:0x4120 + len(image)], 0x120) != image:
        raise SystemExit("The decoded FWSC application does not match the linked image.")
    _, ota_at, ota_size, _, _ = entries[1]
    if decoded[ota_at:ota_at + ota_size] != ota:
        raise SystemExit("The decoded FWSC loader does not match the compiled loader.")
    report = {"product": PRODUCT, "package_board": "smk", "flash_payload_bytes": flash_size, "app_bytes": len(image), "loader_bytes": len(ota),
              "fwsc_bytes": path.stat().st_size, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
              "device_tested": False, "link_checks": notes}
    (OUTPUT / "firmware-build.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    print(path)


if __name__ == "__main__":
    main()
