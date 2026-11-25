#!/usr/bin/env python3
"""Build the ziviSynthiA application against the SMK37 Open V01 Pro SDK."""
import argparse
import base64
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile

from pack_smk37_open import package

ROOT = Path(__file__).resolve().parents[1]
PRODUCT = "SMK-37 Pro_8900822"
VERSION = "0.8.2-alpha2"
BASELINE_FLASH_IMAGE = 466944
NAME = f"ziviSynthiA-SMK37-Pro-Open-{VERSION}"
PINNED = "07f2269d67d5"
MANIFEST_SHA256 = "9956fb209abdf248cfe1e5404ecf1e70eb582439133dbb8510823b39589e65cd"
CODE_FLAGS = (
    "-target", "pi32v2", "-integrated-as", "-mcpu=r3", "-mfprev1",
    "-D__GCC_PI32V2__", "-DSMK37_MODEL_PRO=1", "-fno-common", "-fallow-pointer-null",
    "-fprefer-gnu-section", "-femulated-tls", "-fms-extensions", "-fno-unwind-tables",
    "-ffunction-sections", "-fdata-sections", "-cl-single-precision-constant",
    "-mllvm", "-pi32v2-large-program=true", "-fwrapv", "-fno-fast-math", "-ffp-contract=off",
)
INCLUDES = ("sdk/include", "sdk/include/libc", "app/synth", "sdk/config", "app", "app/board")
NO_LTO = {"device.c", "transfer.c", "midi_router.c", "panel.c", "scan.c", "audio.c"}
HOT = {"audio.c", "audio_output.c", "scan.c", "synth.c", "modules.c", "ks.c"}
LINK_OPTIONS = (
    "mcpu=r3", "-pi32v2-always-use-itblock=false", "-enable-ipra=true",
    "-pi32v2-merge-max-offset=4096", "-pi32v2-enable-simd=true", "-global-merge-on-const",
    "-inline-threshold=5", "-inline-max-allocated-size=32", "-inline-normal-into-special-section=true",
    "-pi32v2-enable-rep-memop", "-emulated-tls", "-pi32v2-large-program=true", "-pi32v2-csync-before-rti=true",
)
KEEP = (
    "connectivity_wl82_bluetooth_start", "audio_app_bluetooth_open", "bt_get_local_name", "bt_get_mac_addr",
)


def replace(path, old, new):
    source = path.read_text()
    if source.count(old) != 1:
        raise ValueError(f"Unexpected SDK contents in {path}: {old}")
    path.write_text(source.replace(old, new))


def verify_sdk(source):
    raw = (source / "sdk/manifest.json").read_bytes()
    if hashlib.sha256(raw).hexdigest() != MANIFEST_SHA256:
        raise ValueError(f"Expected SMK37 Open V01 kit at {PINNED}")
    manifest = json.loads(raw)
    entries = manifest["files"]
    if isinstance(entries, dict):
        entries = [{"path": key, "sha256": value} for key, value in entries.items()]
    for entry in entries:
        name = entry["path"].replace("\\", "/")
        if hashlib.sha256((source / name).read_bytes()).hexdigest() != entry["sha256"].lower():
            raise ValueError(f"SDK checksum mismatch for {name}")


def stage(sdk, target):
    shutil.copytree(sdk, target, copy_function=shutil.copyfile)
    for directory in (target / "app", target / "app/board"):
        directory.chmod(directory.stat().st_mode | 0o700)
    app = target / "app"
    zivi = app / "zivi"
    zivi.mkdir()
    for path in sorted((ROOT / "app/core").iterdir()):
        if path.suffix in (".c", ".h", ".def"):
            shutil.copyfile(path, zivi / path.name)
    for name in ("zivi_smk37.c", "zivi_smk37.h", "format.h", "midi_clock.h"):
        shutil.copyfile(ROOT / "platform/smk37_pro" / name, zivi / name)
    for path in sorted((ROOT / "platform/smk37_open").iterdir()):
        if path.suffix in (".c", ".h"):
            shutil.copyfile(path, zivi / path.name)
    for name in ("factory-presets.inc", "smk37-dsp-tables.inc"):
        shutil.copyfile(ROOT / "build" / name, zivi / name)
    shutil.copyfile(ROOT / "platform/fm1_base/app.c", zivi / "shared_panel.inc")
    replace(zivi / "shared_panel.inc", "../../app/core/presets.h", "presets.h")
    source_header = zivi / "synth.h"
    text = source_header.read_text()
    source_header.write_text(re.sub(r'(?m)^#define ZIVI_VERSION "[^"]+"$', f'#define ZIVI_VERSION "{VERSION}"', text))
    replace(zivi / "presets.c", "../../build/factory-presets.inc", "factory-presets.inc")
    replace(zivi / "dsp_curve.h", "../../build/smk37-dsp-tables.inc", "smk37-dsp-tables.inc")
    replace(zivi / "zivi_smk37.c", ".ram_text.zivi_audio", ".volatile_ram_code.zivi_audio")

    screen = app / "screen.c"
    font = re.search(r"static const uint8_t font\[36\]\[5\] = \{.+?\};", screen.read_text(), re.S)
    if not font:
        raise ValueError("SDK screen font is missing")
    (zivi / "open_font.h").write_text(
        "/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception"
        " | Source: SMK37 Open V01 app/screen.c. */\n" + font[0] + "\n"
    )
    screen.unlink()
    (app / "smk37_controls.c").unlink()

    audio = app / "board/audio.c"
    replace(audio, '#include "audio_output.h"', '#include "audio_output.h"\n#include "zivi/open_device.h"')
    replace(audio, "fm1_engine_render(rendered, FM1_BLOCK_FRAMES)", "zivi_open_audio(rendered, FM1_BLOCK_FRAMES)")
    shutil.copyfile(ROOT / "platform/smk37_open/sdk/audio_output.c", app / "audio_output.c")
    router = app / "midi_router.c"
    replace(router, '#include "fm1_engine_control.h"', '#include "fm1_engine_control.h"\n#include "zivi/open_device.h"')
    replace(router, "    if (clock && !groove_midi_clock_accept(source, m[0])) return;\n    (void)fm1_midi_post(m, n);",
            "    zivi_open_midi(source, m, n);\n    if (clock && !groove_midi_clock_accept(source, m[0])) return;")
    panel = app / "board/panel.c"
    source = panel.read_text()
    start = source.index("    smk37_scan_read();smk37_controls_snapshot(state);")
    end = source.index("\n}\nvoid fm1_led_set", start)
    panel.write_text(source[:start] +
                     "    smk37_scan_read();\n    memset(state, 0, sizeof(*state));\n"
                     "    state->battery = smk37_battery_adc();" + source[end:])
    # Keep the SDK recover path and board clock, while bounding a SPI fault.
    lcd = app / "board/fm1_lcd_hw.h"
    replace(lcd, "    for(unsigned limit=4000000;limit;--limit) {",
            "    uint32_t start=fm1_device_ticks();\n"
            "    for(unsigned limit=4000000;limit && fm1_device_ticks()-start<96000u;--limit) {")
    scan = app / "board/scan.c"
    replace(scan, "for(unsigned count=0;count<256;++count)", "for(unsigned count=0;count<64;++count)")
    version = app / "version.h"
    replace(version, '"V01"', '"V900822"')
    replace(version, '"SMK-37 Pro_8000001"', f'"{PRODUCT}"')


def run(command, **kwargs):
    result = subprocess.run(command, capture_output=True, text=True, **kwargs)
    if result.returncode:
        raise RuntimeError(f"{command[0]} failed\n{result.stdout}{result.stderr}")
    return result.stdout


def link_app(kit, out, toolchain, libm, jobs):
    zivi = kit / "app/zivi"
    files = sorted(p for p in (kit / "app").rglob("*")
                   if p.suffix in (".c", ".S") and "synth" not in p.relative_to(kit / "app").parts)
    sources = sorted(p for p in (kit / "app").rglob("*") if p.is_file())
    digest = hashlib.sha256()
    for path in sources + [kit / "sdk/manifest.json"]:
        digest.update(str(path.relative_to(kit)).encode() + b"\0")
        digest.update(hashlib.sha256(path.read_bytes()).digest())
    for input_file in (Path(__file__), ROOT / "tools/pack_smk37_open.py", libm):
        digest.update(hashlib.sha256(input_file.read_bytes()).digest())
    clang = toolchain / "pi32v2/bin/clang"
    for tool in (clang, toolchain / "pi32v2/bin/ld", toolchain / "common/bin/LLVMgold.so"):
        digest.update(hashlib.sha256(tool.read_bytes()).digest())
    build_id = digest.digest()
    identity = out / "identity.c"
    identity.write_text(
        '#include "layout.h"\n'
        'const struct fm1_firmware_layout fm1_layout = {536576U,8U};\n'
        'const unsigned char fm1_build_id[32] = {' + ",".join(str(n) for n in build_id) + '};\n'
        f'const unsigned char fm1_update_identity[27] = "{PRODUCT}";\n'
    )
    files.append(identity)

    def compile_unit(path):
        name = "_".join(path.relative_to(kit).parts) if path.is_relative_to(kit) else path.name
        obj = out / (name + ".o")
        flags = ["-O2" if path.name in HOT else "-Oz"]
        if path.parent == zivi:
            # Eight blocks cover one missed 10 ms control wake, with margin.
            flags += ["-DZIVI_SMK_NATIVE", "-DZIVI_SMK_CHECK_SDK", "-DZIVI_REAL32",
                      "-DZIVI_SMK_OPEN_AUDIO", "-DZIVI_SMK_RENDER_RATE=44118", "-DZIVI_MAX_RATE=44118",
                      "-DZIVI_SMK_AUDIO_SLOTS=9", "-DZIVI_SMK_MIDI_PORTS=3"]
        if path.name not in NO_LTO:
            flags += ["-flto"]
        try:
            run([clang, *CODE_FLAGS, *[f"-I{p}" for p in INCLUDES], *flags,
                 "-c", os.path.relpath(path, kit), "-o", obj], cwd=kit)
        except RuntimeError as error:
            raise RuntimeError(f"Compiling {path.name}: {error}") from error
        return obj

    with ThreadPoolExecutor(jobs) as pool:
        objects = list(pool.map(compile_unit, files))
    linker = out / "linker.ld"
    linker.write_text((kit / "sdk/config/linker.ld.in").read_text()
                      .replace("@SBC_DECODER@", "sbc_decoder\n")
                      .replace("@ACL_TX_EXTRA@", " + 1024").replace("@ACL_RX_EXTRA@", " + 4096"))
    elf = out / f"{NAME}.elf"
    sdk_lib = kit / "sdk/lib"
    link_map = out / f"{NAME}.map"
    run([toolchain / "pi32v2/bin/ld", "--plugin", toolchain / "common/bin/LLVMgold.so", "-flto",
         *["--plugin-opt=" + flag for flag in LINK_OPTIONS], "-o", elf, *objects,
         "--start-group", "--whole-archive", sdk_lib / "libfm1_engine.a", "--no-whole-archive",
         sdk_lib / "libc.a", sdk_lib / "libcompiler_rt.a", libm, "--end-group",
         "--gc-sections", "-T", linker, "-Map", link_map, *["--undefined=" + symbol for symbol in KEEP]], cwd=kit)
    return elf, link_map, build_id.hex()


def elf_sections(elf):
    data = elf.read_bytes()
    if data[:6] != b"\x7fELF\x01\x01":
        raise ValueError("Expected a 32-bit little-endian SDK ELF")
    start, size, count, string_index = struct.unpack_from("<I10xHHH", data, 32)
    records = [struct.unpack_from("<10I", data, start + size * i) for i in range(count)]
    names = records[string_index]
    strings = data[names[4]:names[4] + names[5]]
    sections = {strings[r[0]:strings.index(b"\0", r[0])].decode(): r for r in records}
    if struct.unpack_from("<I", data, 24)[0] != 0x02000120 or sections[".text"][3] != 0x02000120:
        raise ValueError("Invalid application flash address")
    allowed = {".text", ".data", ".ram0_data", ".cache_ram_data", ".bss",
               ".ram0_bss", ".cache_ram_bss", ".boot_info"}
    for name, entry in sections.items():
        if entry[2] & 2 and entry[5] and name not in allowed:
            raise ValueError(f"Unexpected allocated ELF section {name}")
    return data, sections


def section_image(data, sections):
    return b"".join(data[sections[name][4]:sum(sections[name][4:6])]
                    for name in (".text", ".data", ".ram0_data", ".cache_ram_data"))


def usb_audio_formats(elf_data, sections, symbols):
    text = sections[".text"]
    formats = {}
    for direction, name in (("playback", "spk"), ("capture", "mic")):
        location = text[4] + symbols[f"uac_{name}_as_interface_desc"] - text[3]
        descriptor = elf_data[location:location + 52]
        records = {}
        offset = 0
        while offset < len(descriptor):
            size = descriptor[offset]
            if size < 3 or offset + size > len(descriptor):
                raise ValueError(f"Invalid USB Audio {direction} descriptor")
            record = descriptor[offset:offset + size]
            records[tuple(record[1:3]) if record[1] == 0x24 else record[1]] = record
            offset += size
        form, ep = records[(0x24, 2)], records[5]
        if form[3:8] != bytes((1, 2, 3, 24, 1)) or int.from_bytes(form[8:11], "little") != 44100:
            raise ValueError(f"Unexpected USB Audio PCM contract for {direction}")
        if bool(ep[2] & 0x80) != (direction == "capture"):
            raise ValueError(f"Unexpected USB endpoint direction for {direction}")
        formats[direction] = {"rate": 44100, "channels": 2, "pcm_bits": 24, "endpoint": ep[2]}
    return formats


def verify_symbols(elf, toolchain, elf_data, sections):
    nm = toolchain / "pi32v2/bin/nm"
    result = run([nm, "-n", elf])
    symbols = {parts[-1]: int(parts[0], 16) for line in result.splitlines()
               if len(parts := line.split()) == 3}
    if symbols["_start"] != 0x02000120:
        raise ValueError("Invalid SDK reset vector")
    for name in ("zivi_open_audio", "zivi_smk_irq_read", "fm1_audio_irq", "fm1_audio_copy",
                 "fm1_output_mix", "fm1_usb_audio_read", "fm1_usb_audio_write", "fm1_bluetooth_audio_read",
                 "scan_isr", "scan_soft_isr", "smk37_adc_irq", "key_scan", "encoder_scan", "pad_prc",
                 "keymap", "encodermap", "encoder_delta", "pad_velocity", "smk37_pad_history_step"):
        if not 0x01c00000 <= symbols.get(name, 0) < 0x01c80000:
            raise ValueError(f"{name} must execute in RAM")
    for name in ("audio_ready", "zivi_device", "rendered", "receive", "transmit", "usb_pcm", "bluetooth_pcm",
                 "delayed_pcm"):
        if not 0x01c00000 <= symbols.get(name, 0) < 0x01c80000:
            raise ValueError(f"Audio buffer/control {name} must reside in internal SRAM")
    for name in ("fm1_midi_send_port", "fm1_app_midi_received", "groove_screen_control",
                 "fm1_bluetooth_configure", "fm1_ble_set_role", "fm1_ble_scan", "fm1_ble_connect",
                 "fm1_ble_disconnect", "fm1_update_init", "fm1_update_message", "sbc_decoder"):
        if not symbols.get(name):
            raise ValueError(f"Missing SDK runtime hook {name}")
    for name, minimum, stride in (
        ("initcall", 4, 4), ("early_initcall", 16, 4), ("late_initcall", 4, 4),
        ("platform_initcall", 4, 4), ("module_initcall", 4, 4), ("sys_event_handler", 12, 12),
        ("syscfg_ops", 84, 28), ("lp_target", 72, 8), ("sys_cpu_timer", 16, 16),
        ("clock_critical_handler", 16, 8), ("vfs_ops", 120, 120),
        ("audio_decoder", 56, 56), ("audio_hwaccel", 32, 32),
    ):
        width = symbols[name + "_end"] - symbols[name + "_begin"]
        if width < minimum or width % stride or (name == "vfs_ops" and width != minimum):
            raise ValueError(f"Invalid SDK table {name}")
    for direction, minimum in (("tx", 1024), ("rx", 4096)):
        pool = f"acl_{direction}_pool"
        if symbols[pool + "_end"] - symbols[pool] < minimum:
            raise ValueError(f"Bluetooth controller pool too small: {pool}")
    sizes = {parts[-1]: int(parts[1], 16) for line in run([nm, "-S", elf]).splitlines()
             if len(parts := line.split()) == 4}
    for name, length in (("fm1_update_identity", 27), ("fm1_build_id", 32),
                         ("CurrentTCB", 4), ("highCurrentTCB", 4), ("xIdleTaskHandle", 4)):
        if sizes.get(name) != length:
            raise ValueError(f"Invalid SDK identity or RTOS symbol {name}")
    for name in ("sdp_send_cmd_iotl", "sdp_master_packet_handler",
                 "gatt_client_discover_primary_services_by_uuid128"):
        if sizes.get(name, 0) <= 16:
            raise ValueError(f"Missing real Bluetooth discovery routine {name}")
    if symbols["_MALLOC_SIZE"] < 49152:
        raise ValueError("Less dynamic RAM than the SDK minimum")
    return symbols["_MALLOC_SIZE"], usb_audio_formats(elf_data, sections, symbols)


def write_updater(sdk, output, firmware, report):
    loader = sdk / "sdk/tools/pack/ota.bin"
    meta = dict(schema=1, model="pro", family=8, product=PRODUCT, build_id=report["build_id"],
                bytes=len(firmware), sha256=report["sha256"],
                loader_sha256=hashlib.sha256(loader.read_bytes()).hexdigest(),
                data=base64.b64encode(firmware).decode("ascii"))
    template = (ROOT / "platform/smk37_open/update.html.in").read_text()
    template = template.replace("@@VERSION@@", VERSION)
    content = template.replace("@@WEB_UPDATE@@", (sdk / "sdk/WebUpdate.js").read_text())
    (output / f"{NAME}-atualizador.html").write_text(
        content.replace("@@PACKAGE@@", json.dumps(meta, separators=(",", ":"))))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--open-sdk", type=Path, required=True)
    parser.add_argument("--toolchain", type=Path, required=True)
    parser.add_argument("--libm", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "build/smk37-open")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    sdk, toolchain, libm, output = (path.resolve() for path in
                                    (args.open_sdk, args.toolchain, args.libm, args.output))
    verify_sdk(sdk)
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="build-", dir=output) as temp:
        work = Path(temp)
        kit = work / "sdk"
        stage(sdk, kit)
        elf, link_map, build_id = link_app(kit, work, toolchain, libm, args.jobs)
        elf_data, sections = elf_sections(elf)
        app = section_image(elf_data, sections)
        print(f"Linked Open application: {len(app)} bytes", flush=True)
        heap, usb_audio = verify_symbols(elf, toolchain, elf_data, sections)
        package_bytes, layout = package(app, kit / "sdk/tools/pack", PRODUCT)
        if layout["flash_bytes"] != BASELINE_FLASH_IMAGE:
            raise ValueError("Candidate moves the beginning of VM from the 0.8.0 layout")
        fw = output / f"{NAME}.fwsc"
        fw.write_bytes(package_bytes)
        shutil.copyfile(elf, output / elf.name)
        map_text = link_map.read_text().replace(str(work), "<build>").replace(str(libm), "<libm>/libm.a")
        map_text = re.sub(r"/tmp/lto-llvm-[0-9a-f]+\.o", "<llvm-lto>.o", map_text)
        (output / link_map.name).write_text(map_text)
        report = dict(product=PRODUCT, open_sdk=PINNED, app_bytes=len(app), heap_bytes=heap,
                      build_id=build_id, audio_codec_nominal_hz=44118, usb_audio=usb_audio, sha256=hashlib.sha256(package_bytes).hexdigest(),
                      elf_sha256=hashlib.sha256(elf.read_bytes()).hexdigest(),
                      sdk_manifest_sha256=MANIFEST_SHA256,
                      toolchain={path: hashlib.sha256((toolchain / path).read_bytes()).hexdigest()
                                 for path in ("pi32v2/bin/clang", "pi32v2/bin/ld", "common/bin/LLVMgold.so")},
                      libm_sha256=hashlib.sha256(libm.read_bytes()).hexdigest(),
                      **layout)
        write_updater(kit, output, package_bytes, report)
        (output / "build.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
