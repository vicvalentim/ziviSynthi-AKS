"""Build the SMK-37 Pro bench application on a pinned fm1-base and BSP."""
import argparse
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "platform/smk37_fm1_base"
FM1_COMMIT = "eafac162b4d7bc59176919a36eed179955315f93"
BSP_COMMIT = "a702b840ae2bb7c4fbbd0a168ed509e129b2e9f7"


def snapshot(source, commit, destination):
    version = subprocess.check_output(["git", "-C", source, "rev-parse", "HEAD"], text=True)
    if version.strip() != commit:
        raise ValueError(f"{source}: expected {commit}, got {version.strip()}")
    archive = subprocess.check_output(["git", "-C", source, "archive", commit])
    with tarfile.open(fileobj=io.BytesIO(archive)) as tree:
        tree.extractall(destination, filter="data")


def replace(path, before, after):
    source = path.read_text()
    if source.count(before) != 1:
        raise ValueError(f"Unexpected upstream source in {path}: {before}")
    path.write_text(source.replace(before, after))


def archive_source(kit, destination):
    """Keep the exact modified base and BSP with their original notices."""
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("wb") as raw, gzip.GzipFile(fileobj=raw, mode="wb", filename="", mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode="w") as archive:
            for path in sorted(kit.rglob("*")):
                entry = archive.gettarinfo(str(path), "fm1-base/" + path.relative_to(kit).as_posix())
                entry.uid = entry.gid = entry.mtime = 0
                entry.uname = entry.gname = ""
                if entry.isfile():
                    with path.open("rb") as source:
                        archive.addfile(entry, source)
                else:
                    archive.addfile(entry)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fm1-base", type=Path, required=True)
    parser.add_argument("--sdk", type=Path)
    parser.add_argument("--toolchain", type=Path)
    parser.add_argument("--output", type=Path, default=ROOT / "build/smk37-felucca")
    parser.add_argument("--host", action="store_true")
    parser.add_argument("--source-kit", type=Path, help="export the patched fm1-base and BSP as a source tarball")
    args = parser.parse_args()
    if not args.host and (not args.sdk or not args.toolchain):
        parser.error("a firmware build needs --sdk and --toolchain")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="fm1base-", dir=ROOT / "build") as tmp:
        kit = Path(tmp) / "base"
        snapshot(args.fm1_base, FM1_COMMIT, kit)
        snapshot(args.fm1_base / "bsp", BSP_COMMIT, kit / "bsp")
        # Keep the mixed-case wordmark in the large splash font as well.
        font = kit / "tools/gen_font.py"
        replace(font, '("L", "BarlowSemiCondensed-Bold.ttf", 31, 1, False, 32, 95)',
                      '("L", "BarlowSemiCondensed-Bold.ttf", 31, 1, False, 32, 126)')
        # This bench application keeps panel and BLE settings in RAM.
        # Avoid the upstream first-boot erase of storage owned by other firmware.
        main_c = kit / "device/main_fm1.c"
        replace(main_c, "            smk_store_owner();                      /* the store is this firmware's, or is cleared first (plat_smk37.c) */",
                "            /* Bench: leave the previous firmware's storage intact. */")
        # Let the audio producer run while LCD DMA is busy. A dirty LED
        # frame remains pending until the shared SPI completes its transfer.
        lcd_hw = kit / "bsp/boards/smk37/fm1_lcd_hw.h"
        replace(lcd_hw, "FM1_INLINE void fm1_lcd_spi_off(void)",
                "FM1_INLINE int fm1_lcd_dma_ready(void) { return (FM1_LCD_SPI_CON & 0x8000u) != 0; }\n\n"
                "FM1_INLINE void fm1_lcd_spi_off(void)")
        strip = kit / "device/plat_smk37.c"
        replace(strip, "    if (!smk_pad_dirty)",
                "    if (!smk_pad_dirty || (lcd_busy && !fm1_lcd_dma_ready()) || !app_led_flush_ready())")
        replace(main_c, """#if PLAT_SMK37
            smk_pads_flush();                       /* the pad LEDs, if a colour changed: one frame between LCD transfers */
#endif
        } else {
            fm1b_dbg.stage = 6;
            app_input_only();
        }""", """        } else {
            fm1b_dbg.stage = 6;
            app_input_only();
        }
#if PLAT_SMK37
        smk_pads_flush();                           /* only once the existing SPI transfer has finished */
#endif""")
        mix = kit / "device/uac_mix.h"
        replace(mix, "    uac_play(o, n, full ? play_q12 : (master * master >> 12) * play_q12 >> 12);   /* the computer's audio, at MASTER */",
                """    if (app_usb_matrix_active()) {
        int32_t *input = n == APP_BLOCK ? app_usb_input_buffer() : NULL;
        if (input) {
            memset(input, 0, 2u * n * sizeof *input);
            uac_play(input, n, 4096u);
            app_usb_input_ready();
        } else {
            uac_play(o, n, 0);                         /* drain even if the app missed its window */
        }
    } else {
        uac_play(o, n, full ? play_q12 : (master * master >> 12) * play_q12 >> 12);
    }""")
        replace(mix, "        const float g = (float)gain * (1.0f / 4096.0f);\n"
                "        uint32_t i;\n"
                "        for (i = 0; i < 2u * n; i++)\n"
                "            o[i] = (int32_t)((float)o[i] * g);",
                "        app_limit_master(o, gain);")
        replace(kit / "host/plat_host.c", "        const float g = (float)APP_GAIN(master) * (1.0f / 4096.0f);\n"
                "        for (k = 0; k < 2u * APP_BLOCK; k++)\n"
                "            blk[k] = (int32_t)((float)blk[k] * g);",
                "        app_limit_master(blk, APP_GAIN(master));")
        # Preserve the application's physical K5 cursor and global K8 master.
        smk = kit / "device/plat_smk37.c"
        replace(smk, "#define SMK_MASTER_ENC 4u", "#define SMK_MASTER_ENC 7u")
        replace(smk, "    if (s) {\n        smk_master", "    if (s && !app_master_turn(s)) {\n        smk_master")
        panel = kit / "device/panel.c"
        replace(panel, "#define PANEL_ENC_RESERVED (1u << 4)", "#define PANEL_ENC_RESERVED (1u << 7)")
        replace(panel, "    {5, 6, 7, 0, 1, 2, 3},", "    {5, 6, 4, 0, 1, 2, 3},")
        platform = kit / "device/plat_fm1.c"
        with platform.open("a") as source:
            source.write("\nvoid app_usb_platform_fence(void) { RING_PUBLISH(); }\n")
        replace(platform, "    if (to & (TO_USB | TO_TRS))", "    if (to & TO_TRS)")
        host = kit / "host/plat_host.c"
        replace(host, "static void sim_render(int32_t *blk)", """static unsigned sim_usb_source;

static void sim_usb_input(void)
{
    static unsigned phase;
    if (!app_usb_matrix_active()) return;
    int32_t *input = app_usb_input_buffer();
    if (!input) return;
    for (unsigned i = 0; i < APP_BLOCK; ++i, ++phase) {
        input[2 * i] = (sim_usb_source & 1u) ? ((phase & 32u) ? 1100000 : -1100000) : 0;
        input[2 * i + 1] = (sim_usb_source & 2u) ? ((phase & 8u) ? 640000 : -640000) : 0;
    }
    app_usb_input_ready();
}

static void sim_render(int32_t *blk)""")
        replace(host, "    app_render(blk, APP_BLOCK, uac_full ? 4096u : APP_GAIN(master));",
                "    app_render(blk, APP_BLOCK, uac_full ? 4096u : APP_GAIN(master));\n"
                "    sim_usb_input();")
        replace(host, "    to &= usbh_to_usb() | ble_listening();",
                "    to &= usbh_to_usb() | ble_listening() | TO_TRS;")
        replace(host, '        } else if (!strcmp(cmd, "spin")) {',
                '        } else if (!strcmp(cmd, "spin") && !strcmp(a, "K8")) {\n'
                '            int turn = atoi(b);\n'
                '            if (!app_master_turn(turn)) master = (uint32_t)clamp((int)master + turn * 96, 0, 4096);\n'
                '            run_ms(1);\n'
                '        } else if (!strcmp(cmd, "spin")) {')
        replace(host, '        else if (!strcmp(cmd, "shot")) {',
                '        else if (!strcmp(cmd, "splash")) app_splash();\n'
                '        else if (!strcmp(cmd, "usbsource")) sim_usb_source = (unsigned)atoi(a);\n'
                '        else if (!strcmp(cmd, "shot")) {')
        # User slots use three A/B pairs in the board's unallocated window.
        # Keep existing stock sectors, panel calibration and BLE records read-only.
        unity = kit / "device/unity.c"
        replace(unity, "static int st_erase(uint32_t off)", """_Static_assert(APP_USER_STORE_LO == FL_FREE_LO && APP_USER_STORE_HI == FL_FREE_HI,
               "The user store must stay in the SMK's free sectors");
static int user_store_sector(uint32_t off)
{
    uint8_t bytes[64];
    if (st_read(off + 256, bytes, 4)) return 0;
    if (!memcmp(bytes, APP_USER_TAG, 4)) return 1;
    for (uint32_t at = 0; at < 4096; at += sizeof bytes) {
        if (st_read(off + at, bytes, sizeof bytes)) return 0;
        for (uint32_t i = 0; i < sizeof bytes; ++i)
            if (bytes[i] != 0xffu) return 0;
    }
    return 1;
}

static int st_erase(uint32_t off)""")
        replace(unity, "    if (!FL_STORE_OK(off, 0x1000u))",
                "    if (!FL_IN(off, 0x1000u, FL_FREE_LO, FL_FREE_HI) || !user_store_sector(off))")
        replace(unity, "    if (!FL_STORE_OK(off, n))",
                "    if (!FL_IN(off, n, FL_FREE_LO, FL_FREE_HI))")
        for signature, result in (
            ("flash_ok ? st_load(obj, dst, max) : -1", "flash_ok && obj >= OBJ_USER_1 && obj < OBJ_NOBJ ? st_load(obj, dst, max) : -1"),
            ("flash_ok ? st_load_head(obj, dst, n) : -1", "flash_ok && obj >= OBJ_USER_1 && obj < OBJ_NOBJ ? st_load_head(obj, dst, n) : -1"),
            ("flash_ok ? st_size(obj) : -1", "flash_ok && obj >= OBJ_USER_1 && obj < OBJ_NOBJ ? st_size(obj) : -1"),
            ("flash_ok ? st_save(obj, src, len) : -9", "flash_ok && obj >= OBJ_USER_1 && obj < OBJ_NOBJ ? st_save(obj, src, len) : -9"),
        ):
            replace(platform, signature, result)

        # Isolate the wide multiplies that may race adjacent loads on PI32v2.
        ecc = kit / "bt/br/uECC.c"
        replace(ecc, "static void muladd(uECC_word_t a,", """static __attribute__((noinline)) uECC_dword_t smk_ecc_product(uECC_word_t a, uECC_word_t b)
{
    return (uECC_dword_t)a * b;
}

static void muladd(uECC_word_t a,""")
        replace(ecc, "    uECC_dword_t p = (uECC_dword_t)a * b;\n"
                "    uECC_dword_t r01 = ((uECC_dword_t)(*r1) << uECC_WORD_BITS) | *r0;\n"
                "    r01 += p;",
                "    uECC_dword_t p = smk_ecc_product(a, b);\n"
                "    uECC_dword_t r01 = ((uECC_dword_t)(*r1) << uECC_WORD_BITS) | *r0;\n"
                "    r01 += p;")
        dct = kit / "bt/br/sbc/sbc_dct.c"
        replace(dct, "#define SBC_IDCT_MULT(a, b, c) SBC_MULT_32_16_SIMPLIFIED(a, b, c)",
                """static __attribute__((noinline)) int32_t smk_sbc_dct_product(int32_t a, int32_t b)
{
    return (int32_t)(((int64_t)a * b) >> 15);
}
#define SBC_IDCT_MULT(a, b, c) ((c) = smk_sbc_dct_product((a), (b)))""")

        build = kit / "tools/build.py"
        replace(build, "    have = firmware_json.parts(CONF, BLE, OWN, WIFI, CPU2, touch, words)",
                "    have = firmware_json.parts(CONF, BLE, OWN, WIFI, CPU2, touch, words)\n"
                '    have.update(velocity=True, pressure=True)  # Native SMK key and pad inputs')
        replace(build, '    h = hashlib.sha1((VERSION or "").encode())',
                '    h = hashlib.sha1((VERSION or "").encode())\n'
                '    h.update((APP.parents[1] / "build/felucca-presets.inc").read_bytes())\n'
                '    h.update((APP.parent / "module_panel.def").read_bytes())')
        replace(build, 'for d in (APP, SRC / "device", SRC / "include", BSP / "chip"):',
                'for d in (APP, APP.parents[1] / "app/core", APP.parent / "fm1_base", APP.parent / "smk37_pro", '
                'SRC / "device", SRC / "include", BSP / "chip", BSP / "boards/smk37"):')
        replace(build, 'f.suffix in (".c", ".h", ".S", ".toml", ".ld")',
                'f.suffix in (".c", ".h", ".S", ".toml", ".ld", ".def")')
        replace(build, '"-mfprev1", "-ffp-contract=off"', '"-mfprev1", "-ffp-contract=off", "-cl-single-precision-constant"')
        replace(build, "*CFLAGS, *FPU, *board_inc()", '*CFLAGS, *FPU, "-isystem", os.environ["ZIVI_NEWLIB_INCLUDE"], *board_inc()')
        replace(build, '*b_libs, "-o", elf)', '*b_libs, "--start-group", os.environ["ZIVI_LIBM"], '
                'os.environ["ZIVI_COMPILER_RT"], "--end-group", "-Map", OUT / (NAME + ".map"), "-o", elf)')
        if args.source_kit:
            archive_source(kit, args.source_kit)
        env = dict(os.environ, FM1B_BOARD="smk37", FM1B_MODEL="pro")
        if not args.host:
            sdk = args.sdk.resolve()
            libraries = sdk / "include_lib/newlib/pi32v2-lib"
            env.update(JIELI_TOOLCHAIN=str(args.toolchain.resolve()),
                       ZIVI_LIBM=str(libraries / "libm.a"),
                       ZIVI_NEWLIB_INCLUDE=str(sdk / "include_lib/newlib/include"),
                       ZIVI_COMPILER_RT=str(libraries / "libcompiler_rt.a"))
        command = [sys.executable, build, APP]
        command += ["--host"] if args.host else ["--sdk", sdk]
        subprocess.run(command, env=env, check=True)
        if not args.host:
            names = ("zivisynthi-felucca.elf", "zivisynthi-felucca.bin",
                     "zivisynthi-felucca-smk37.fwsc", "firmware-smk37.json")
            for name in names:
                source = APP / "build" / name
                if source.suffix == ".elf":
                    # This clang writes its input path to the ELF file symbol.
                    # Keep its string-table offsets and all loadable bytes.
                    symbol = os.fsencode(kit.relative_to(ROOT)) + b"/device/unity.c\x00"
                    elf = source.read_bytes()
                    if elf.count(symbol) != 1:
                        raise ValueError("Expected one temporary device unity file symbol")
                    label = b"fm1-base/device/unity.c"
                    (output / name).write_bytes(elf.replace(symbol, label.ljust(len(symbol) - 1, b"\0") + b"\0"))
                else:
                    shutil.copyfile(source, output / name)
            link_map = "zivisynthi-felucca.map"
            contents = (APP / "build" / link_map).read_text()
            contents = contents.replace(str(kit.relative_to(ROOT)), "<fm1-base>").replace(str(libraries), "<newlib>")
            (output / link_map).write_text(contents)
            toolchain = args.toolchain.resolve()
            hashes = {name: hashlib.sha256((output / name).read_bytes()).hexdigest() for name in (*names, link_map)}
            compiler = toolchain / "pi32v2/bin/clang"
            report = {
                "application": "ziviSynthi AKS · Felucca Pro 3.9.5",
                "model": "SMK-37 Pro",
                "identity": "SMK-37 Pro_99400",
                "fm1_base": FM1_COMMIT,
                "felucca_bsp": BSP_COMMIT,
                "codec_nominal_hz": 44118,
                "usb_audio": {"rate": 44100, "channels": 2, "bits": 16},
                "physical_validation": False,
                "app_window": ["0x04000", "0x9D000"],
                "compiler_sha256": hashlib.sha256(compiler.read_bytes()).hexdigest(),
                "newlib_sha256": {name: hashlib.sha256((libraries / name).read_bytes()).hexdigest()
                                  for name in ("libm.a", "libcompiler_rt.a")},
                "sha256": hashes,
            }
            (output / "build.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")


if __name__ == "__main__":
    main()
