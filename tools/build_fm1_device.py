"""Build a dedicated FM-1 image from the project's fm1-base panel and DSP."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from build_smk37_felucca import ROOT, archive_source, replace, snapshot

FM1_COMMIT = "2bb8e74f2962475eeeefd5179420bd8d73a3da6c"
BSP_COMMIT = "2bd9dbe1fcbb8c67ba6721b2435f3fb0597a202a"
VERSION = "0.1.0-alpha1"
APP = ROOT / "platform/fm1_device"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fm1-base", type=Path, required=True)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--toolchain", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    sdk = args.sdk.resolve()
    libraries = sdk / "include_lib/newlib/pi32v2-lib"
    args.output.mkdir(parents=True, exist_ok=True)
    generated = APP / "build" / "fm1-panel.inc"
    generated.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ROOT / "platform/fm1_base/app.c", generated)
    replace(generated, '#include "../../app/core/presets.h"', "// The FM-1 wrapper includes the DSP declarations.")
    replace(generated, "synth_init(&zivi, 44100.0);", "synth_init(&zivi, 44118.0f);")
    replace(generated, "double monitor", "float monitor")
    replace(generated, "fabs((double)a), fabs((double)c)", "fabs((float)a), fabs((float)c)")
    with tempfile.TemporaryDirectory(prefix="fm1-native-", dir=ROOT / "build") as work:
        kit = Path(work) / "base"
        snapshot(args.fm1_base, FM1_COMMIT, kit)
        snapshot(args.fm1_base / "bsp", BSP_COMMIT, kit / "bsp")
        firmware_metadata = kit / "tools/firmware_json.py"
        metadata_root = '    return app.parent if app.name == "app" and app.parent != Path(src).resolve() else app'
        replace(firmware_metadata, metadata_root,
                '    if app.name == "fm1_device" and app.parent.name == "platform":\n'
                '        return app.parents[1]\n' + metadata_root)
        builder = kit / "tools/build.py"
        replace(builder, 'flags = [*CFLAGS, *FPU, *inc(BSP / "chip", BSP / "boards" / "fm1", SRC',
                'flags = [*CFLAGS, *FPU, "-isystem", os.environ["ZIVI_FM1_NEWLIB"], '
                '"-DZIVI_REAL32", "-DZIVI_MAX_RATE=44118", "-cl-single-precision-constant", '
                '*inc(BSP / "chip", BSP / "boards" / "fm1", SRC')
        replace(builder, '*b_libs, "-o", elf)',
                '*b_libs, "--start-group", os.environ["ZIVI_FM1_LIBM"], '
                'os.environ["ZIVI_FM1_COMPILER_RT"], "--end-group", "-o", elf)')
        replace(builder, 'for d in (APP, SRC / "device", SRC / "include", BSP / "chip"):',
                'for d in (APP, APP.parent / "fm1_base", APP.parent / "smk37_pro", '
                'APP.parents[1] / "app/core", SRC / "device", SRC / "include", BSP / "chip"):')
        env = dict(os.environ, AC79_SDK=str(sdk), JIELI_TOOLCHAIN=str(args.toolchain.resolve()),
                   JIELI_DOCKER="0", ZIVI_FM1_NEWLIB=str(sdk / "include_lib/newlib/include"),
                   ZIVI_FM1_LIBM=str(libraries / "libm.a"),
                   ZIVI_FM1_COMPILER_RT=str(libraries / "libcompiler_rt.a"))
        subprocess.run([sys.executable, builder, APP, "--release", VERSION], cwd=ROOT, env=env, check=True)
        for name in (f"zivisynthi-fm1-{VERSION}.fwsc", "firmware.json"):
            shutil.copyfile(APP / "build" / name, args.output / name)
        archive_source(kit, args.output / "fm1-base-source.tar.gz")


if __name__ == "__main__":
    main()
