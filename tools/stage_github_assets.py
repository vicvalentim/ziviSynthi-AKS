"""Stage checked SMK-37 Pro assets for GitHub Actions or a manual release."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
from zipfile import ZIP_DEFLATED, ZipFile

ROOT = Path(__file__).resolve().parents[1]


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("source_kit", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--rollback", type=Path)
    args = parser.parse_args()

    report_path = args.build / "build.json"
    native_path = args.build / "firmware-smk37.json"
    fwsc = args.build / "zivisynthi-felucca-smk37.fwsc"
    report = read_json(report_path)
    native = read_json(native_path)
    catalog = read_json(ROOT / "firmware.json")
    packages = read_json(ROOT / "platform/web/firmware-packages.json")
    sha = hashlib.sha256(fwsc.read_bytes()).hexdigest()
    build_version = re.search(r"\b(\d+\.\d+\.\d+)$", report["application"])
    if not build_version:
        parser.error("build.json: versão de firmware inválida")
    version = build_version[1]
    if (report["model"] != "SMK-37 Pro" or native["hardware"]["models"] != ["SMK-37 Pro"] or
            packages.get(version) != {"id": report["identity"], "sha256": sha} or
            report["sha256"].get(fwsc.name) != sha):
        parser.error("o FWSC não corresponde à identidade e ao SHA-256 fixados do SMK-37 Pro")
    for key in ("schema", "id", "name", "author", "hardware"):
        if catalog[key] != native[key]:
            parser.error(f"firmware.json e metadados do build divergem em {key}")
    screenshot = ROOT / catalog["screenshot"]
    header = screenshot.read_bytes()[:24]
    if (header[:16] != b"\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR" or header[16:20] != header[20:24] or
            int.from_bytes(header[16:20], "big") < 240 or screenshot.stat().st_size > 2 * 1024 * 1024):
        parser.error("firmware.json exige PNG quadrado de pelo menos 240 px e até 2 MiB")

    args.output.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(fwsc, args.output / f"{catalog['id']}-smk37-{version}.fwsc")
    shutil.copyfile(ROOT / "firmware.json", args.output / "firmware-smk37.json")
    shutil.copyfile(report_path, args.output / "build.json")
    shutil.copyfile(args.source_kit, args.output / "fm1-base-source.tar.gz")
    shutil.copyfile(ROOT / "NOTICES.md", args.output / "NOTICES.md")
    shutil.copyfile(ROOT / "LICENSE", args.output / "LICENSE")
    editor = [sys.executable, ROOT / "tools/build_web.py", args.output / "zivisynthi-aks-editor.html",
              "--embed-firmware", version, fwsc]
    if args.rollback:
        older = list(packages)[list(packages).index(version) + 1]
        editor.extend(["--embed-firmware", older, args.rollback])
    subprocess.run(editor, cwd=ROOT, check=True)
    with tempfile.TemporaryDirectory(prefix="zivi-presets-") as temp:
        patches = Path(temp)
        for name, option in (("presets", "--felucca"), ("presets-usb", "--usb-effects")):
            subprocess.run([sys.executable, ROOT / "tools/build_presets.py", option,
                            "--json-dir", patches / name], cwd=ROOT, check=True)
        with ZipFile(args.output / "zivisynthi-aks-presets.zip", "w", ZIP_DEFLATED) as bank:
            for path in sorted(patches.glob("*/*.json")):
                bank.write(path, path.relative_to(patches).as_posix())


if __name__ == "__main__":
    main()
