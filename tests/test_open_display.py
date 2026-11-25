"""Compile the actual Open renderer against a simulated SDK SPI/DMA boundary."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from build_smk37_open import stage, verify_sdk

sdk, compiler = Path(sys.argv[1]).resolve(), sys.argv[2]
verify_sdk(sdk)
with tempfile.TemporaryDirectory() as temp:
    stage(sdk, Path(temp) / "sdk")
    unit = ROOT / "tests/test_open_display.c"
    binary = Path(temp) / "test-open-display"
    subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
                    "-O2", "-I", str(Path(temp) / "sdk/app/zivi"), str(unit), "-o", str(binary)], check=True)
    subprocess.run([binary], check=True)

    sdk_root = Path(temp) / "sdk"
    shim = Path(temp) / "smk37_hw.h"
    shim.write_text(
        "#include <stdint.h>\n"
        "uint32_t fm1_device_ticks(void);\n"
        "void smk37_scan_read(void);\n"
        "void smk37_rgb_set(unsigned id, unsigned rgb);\n"
        "void smk37_rgb_service(void);\n"
    )
    sdk_includes = [sdk_root / sub for sub in ("app/zivi", "app/synth", "app", "sdk/include")]
    sources = [sdk_root / "app/zivi" / name for name in ("synth.c", "ks.c", "modules.c", "presets.c", "zivi_smk37.c")]
    panel = Path(temp) / "test-open-panel"
    subprocess.run([compiler, "-std=c11", "-O2", "-ffunction-sections", "-fdata-sections",
                    "-DFM1_HOST_TEST", "-DZIVI_REAL32", "-fsingle-precision-constant",
                    "-DZIVI_MAX_RATE=44118", "-DZIVI_SMK_RENDER_RATE=44118", "-DZIVI_SMK_OPEN_AUDIO",
                    "-DZIVI_SMK_AUDIO_SLOTS=9", "-DZIVI_SMK_MIDI_PORTS=3",
                    "-I", temp, *[f"-I{include}" for include in sdk_includes], *map(str, sources),
                    str(ROOT / "tests/test_open_panel.c"), "-Wl,--gc-sections", "-lm", "-o", str(panel)], check=True)
    args = [panel]
    if screens := os.environ.get("SMK_OPEN_SCREENSHOTS"):
        Path(screens).mkdir(parents=True, exist_ok=True)
        args.append(screens)
    subprocess.run(args, check=True)

    mix = Path(temp) / "test-open-mixer"
    subprocess.run([compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-DFM1_HOST_TEST",
                    "-I", str(sdk_root / "sdk/include"), str(sdk_root / "app/audio_output.c"),
                    str(ROOT / "tests/test_open_mixer.c"), "-o", str(mix)], check=True)
    subprocess.run([mix], check=True)
