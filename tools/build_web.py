"""Gera o HTML autônomo e usa o catálogo de matriz compartilhado com C."""
import argparse
import base64
import hashlib
import json
import re
from pathlib import Path

from build_presets import load as load_presets
from build_vernier import load as load_verniers

project = Path(__file__).resolve().parents[1]
web = project / "platform" / "web"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("output", type=Path)
parser.add_argument("--embed-firmware", action="append", nargs=2, metavar=("VERSION", "FWSC"), default=[])
args = parser.parse_args()


def read(name):
    return (web / name).read_text(encoding="utf-8")


def catalog():
    data = {"SOURCE": [None] * 16, "DEST": [None] * 16}
    pattern = re.compile(r'(SOURCE|DEST)\((\w+),\s*(\d+),\s*"([^"]+)",\s*(-?\d+)\)')
    for line in (project / "app/core/matrix.def").read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        group, key, coord, label, stage = pattern.fullmatch(line).groups()
        index = int(coord)
        if data[group][index] is not None:
            raise ValueError(f"Endereço duplicado {group} {index}")
        data[group][index] = [key, label, int(stage)]
    if any(None in group for group in data.values()):
        raise ValueError("O catálogo precisa conter 16 linhas e 16 colunas")
    return "const MatrixCatalog = " + json.dumps(data, ensure_ascii=True) + ";\n"


def parameters():
    data = []
    pattern = re.compile(r'PARAM\(\s*\w+,\s*(\w+),\s*(\d+),\s*"([^"]+)",\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+)\)')
    for line in (project / "app/core/parameters.def").read_text(encoding="utf-8").splitlines():
        key, group, label, low, high, initial, step = pattern.fullmatch(line).groups()
        data.append({"id": key, "group": int(group), "label": label, "lo": float(low),
                     "hi": float(high), "initial": float(initial), "step": float(step)})
    return "const ModuleCatalog = " + json.dumps(data, ensure_ascii=True) + ";\n"


def scales():
    data = []
    pattern = re.compile(r'SCALE\((\w+)\s*,\s*(0x[0-9A-F]{3}),\s*"([^"]+)",\s*"[^"]+",\s*"([^"]+)"\)')
    for line in (project / "app/core/scales.def").read_text(encoding="utf-8").splitlines():
        symbol, mask, name, group = pattern.fullmatch(line).groups()
        data.append({"id": symbol, "mask": int(mask, 16), "name": name, "group": group})
    return "const ScaleCatalog = " + json.dumps(data, ensure_ascii=True) + ";\n"


def panel_catalog():
    controls = {
        "QUICK_SHAPE": "shape", "QUICK_TUNE": "tune", "QUICK_MIX": "mix",
        "QUICK_LFO": "lfo", "QUICK_CUTOFF": "cutoff", "QUICK_LEVEL": "level",
    }
    for line in (project / "app/core/parameters.def").read_text(encoding="utf-8").splitlines():
        symbol, name = re.match(r"PARAM\((\w+),\s*(\w+),", line).groups()
        controls[symbol] = name

    racks = {}
    used = set()
    for line in (project / "platform/module_panel.def").read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        if line.startswith("RACK("):
            key, title, tab, hue = re.fullmatch(r'RACK\((\w+), "([^"]+)", "([^"]+)", (\w+)\)', line).groups()
            if key in racks:
                raise ValueError(f"Módulo duplicado: {key}")
            racks[key] = {"title": title, "tab": tab, "color": hue.lower(), "dials": [None] * 8}
        else:
            rack, slot, symbol, label, hue, kind = re.fullmatch(
                r'DIAL\((\w+), ([1-8]), (\w+), "([^"]+)", (\w+), (\w+)\)', line).groups()
            dials = racks[rack]["dials"]
            slot = int(slot) - 1
            if dials[slot] or symbol in used:
                raise ValueError(f"Controle duplicado: {rack} K{slot + 1}, {symbol}")
            dials[slot] = {"id": controls[symbol], "label": label, "color": hue.lower(), "format": kind}
            used.add(symbol)
    if used != controls.keys():
        raise ValueError(f"Painel incompleto: {set(controls) - used}")
    return "const PanelCatalog = " + json.dumps(list(racks.values())) + ";\n"


def vernier_catalog():
    return "const VernierCatalog = " + json.dumps(load_verniers()) + ";\n"


html = read("index.html")
factory = {key: dict(entry.patch, play=entry.play, bank=entry.bank) for key, entry in load_presets(felucca=True).items()}
calibration = (project / "app/core/aks_calibration.def").read_text(encoding="utf-8")
aks_catalog = "const AKSCalibration = " + json.dumps({
    name: float(value) for name, value in re.findall(r"AKS_CALIBRATION\((\w+), ([\d.]+)\)", calibration)
}) + ";\n"
dsp = catalog() + parameters() + scales() + aks_catalog + "const FactoryCatalog = " + json.dumps(factory, ensure_ascii=True) + ";\n" + read("ks.js") + read("modules.js") + read("dsp.js")
worklet = json.dumps(dsp + "\n" + read("worklet.js")).replace("<", "\\u003c")
def embedded_fonts():
    faces = []
    for weight, style in ((500, "Medium"), (700, "Bold")):
        path = web / "fonts" / f"BarlowSemiCondensed-{style}.ttf"
        encoded = base64.b64encode(path.read_bytes()).decode("ascii")
        faces.append(f'@font-face {{ font-family: "Barlow Panel"; font-weight: {weight}; '
                     f'font-display: swap; src: url(data:font/ttf;base64,{encoded}) format("truetype"); }}')
    license_text = read("fonts/Barlow-OFL.txt")
    return "/*\n" + license_text + "\n*/\n" + "\n".join(faces) + "\n"

def firmware_assets():
    packages = json.loads(read("firmware-packages.json"))
    embedded = {}
    for version, path in args.embed_firmware:
        raw = Path(path).read_bytes()
        if version not in packages or hashlib.sha256(raw).hexdigest() != packages[version]["sha256"]:
            raise ValueError(f"FWSC diferente do manifesto para Felucca {version}")
        embedded[version] = base64.b64encode(raw).decode("ascii")
    return ("const FirmwarePackages = " + json.dumps(packages) + ";\n"
            "const EmbeddedFirmware = " + json.dumps(embedded) + ";\n")


def firmware_transport():
    # Keep upstream modules in one scope so the stand-alone HTML runs from disk.
    sources = []
    for name in ("fm1ports.js", "fm1pkg.js", "fm1ota.js"):
        source = read("felucca-updater/" + name)
        source = re.sub(r"^import .*$", "", source, flags=re.M)
        source = re.sub(r"^export (?=(?:async )?(?:function|const|class) )", "", source, flags=re.M)
        sources.append(source)
    return ("const FeluccaFirmwareTransport = (() => {\n" + "\n".join(sources)
            + "\nreturn {Updater, IS_BLUETOOTH, pack7, parseFrame, productOf, logicalImage, "
              "checkUfw, ufwEntries, hasLoader, sha256hex};\n})();\n")


parts = {
    "STYLE": embedded_fonts() + read("skin.css") + read("aks.css"),
    "DSP": dsp,
    "UI": "const ZIVI_WORKLET = " + worklet + ";\n" + panel_catalog() + vernier_catalog()
          + firmware_assets() + firmware_transport() + read("visuals.js") + read("studio.js")
          + read("sequence.js") + read("firmware.js") + read("ui.js"),
}
for name, source in parts.items():
    marker = "/* " + name + " */"
    if html.count(marker) != 1:
        raise ValueError(f"Esperado um marcador {marker}")
    html = html.replace(marker, source, 1)
args.output.write_text(html, encoding="utf-8")
