"""Build the native catalog and individual portable patches from one sound bank."""
import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path

CORE = Path(__file__).resolve().parents[1] / "app/core"
BANKS = {"studio": "factory_presets.json", "lab": "lab_presets.json",
         "performance": "performance_presets.json", "texture": "texture_presets.json",
         "sequence": "sequence_presets.json"}
FELUCCA_BANKS = {
    "instrument": "instrument_presets.json",
    "handbook": "handbook_presets.json",
    "handbook_workshop": "handbook_workshop_presets.json",
    "keys": "keys_presets.json",
    "exploration": "exploration_presets.json",
}
PANEL_BANKS = {
    "ESTUDIO / LAB": ("studio", "lab"),
    "PERFORMANCE": ("performance",),
    "TEXTURAS": ("texture",),
    "SEQUENCIAS KS": ("sequence",),
    "INSTRUMENTO": ("instrument",),
    "HANDBOOK / I": ("handbook",),
    "HANDBOOK / II": ("handbook_workshop",),
    "TECLAS & BAIXOS": ("keys",),
    "EXPLORACAO": ("exploration",),
    "USB FX": ("usb_effects",),
}
DEFAULTS = {"tune": 0, "shape": 0.5, "level": 0.7, "mix": 0, "lfo": 3.5, "cutoff": 1200}
RANGES = {"tune": (-24, 24), "shape": (0, 1), "level": (0, 1), "mix": (0, 1),
          "lfo": (0.02, 500), "cutoff": (5, 10000)}


@dataclass(frozen=True)
class FactoryEntry:
    bank: str
    display: str
    play: str
    patch: dict
    basic: dict
    knobs: list


def load(*, felucca=False, usb_effects=False):
    modules = {}
    for line in (CORE / "parameters.def").read_text().splitlines():
        enum, key, lo, hi, default = re.fullmatch(
            r'PARAM\((\w+),\s*(\w+),\s*\d+,\s*"[^"]+",\s*([^,]+),\s*([^,]+),\s*([^,]+),.*\)', line
        ).groups()
        modules[key] = (enum, float(lo), float(hi), float(default))
    result = {}
    banks = {**BANKS, **FELUCCA_BANKS} if felucca else BANKS
    if usb_effects:
        banks = {"usb_effects": "usb_effects.json"}
    entries = ((bank, item) for bank, filename in banks.items()
               for item in json.loads((CORE / filename).read_text(encoding="utf-8")))
    for bank, item in entries:
        key = item["key"]
        if key in result or not re.fullmatch(r"[a-z\d]+(?:-[a-z\d]+)*", key):
            raise ValueError(key)
        overrides = item["controls"]
        basic = {**DEFAULTS, **{k: v for k, v in overrides.items() if k in DEFAULTS}}
        controls = {**basic, **{k: info[3] for k, info in modules.items()}}
        controls.update(sineLevel=1 - basic["mix"], rampLevel=basic["mix"])
        knobs = []
        for k, v in overrides.items():
            lo, hi = RANGES[k] if k in RANGES else modules[k][1:3]
            if type(v) not in (int, float) or not lo <= v <= hi or (k in ("envMode", "triggerMode") and int(v) != v):
                raise ValueError((key, k, v))
            controls[k] = v
            if k in modules:
                knobs.append((modules[k][0], v))
        pins = []
        for coord in item["routes"].split():
            col, row, half = re.fullmatch(r"([A-P])(\d{1,2})(/2)?", coord).groups()
            pin = [int(row) - 1, ord(col) - ord("A"), 2 if half else 1]
            if not 0 <= pin[0] < 16 or any(p[:2] == pin[:2] for p in pins):
                raise ValueError((key, coord))
            pins.append(pin)
        usb_routes = []
        if bank == "usb_effects":
            for row, col, amount in pins:
                if row not in (7, 8):
                    continue
                usb_routes.append([row - 7, col, amount])
            pins = [pin for pin in pins if pin[0] not in (7, 8)]
        ks_routes = []
        for coord in item.get("ksRoutes", "").split():
            col, half = re.fullmatch(r"([A-P])(/2)?", coord).groups()
            route = [ord(col) - ord("A"), 2 if half else 1]
            if any(old[0] == route[0] for old in ks_routes):
                raise ValueError((key, coord))
            ks_routes.append(route)
        if len(pins) > 16:
            raise ValueError((key, "native catalog capacity"))
        ks_matrix = item.get("ksMatrix", False)
        if type(ks_matrix) is not bool or (ks_matrix and ks_routes):
            raise ValueError((key, "ksMatrix"))
        patch = {"version": 3, "name": item["name"], "controls": controls, "pins": pins,
                 "ksRoutes": ks_routes, "ksMatrix": ks_matrix,
                 "sequenceTrigger": "notes" in item}
        if usb_routes:
            patch["usbRoutes"] = usb_routes
        if "notes" in item:
            length = item["lengthMs"]
            events = []
            if type(length) is not int or not 1 <= length <= 30000 or not 1 <= len(item["notes"]) <= 128:
                raise ValueError((key, length))
            for start, note, velocity, duration in item["notes"]:
                end = start + duration
                if any(type(v) is not int for v in (start, note, velocity, duration)) or not (
                    0 <= start < end < length and 0 <= note <= 127 and 1 <= velocity <= 127
                    and (not events or events[-1][0] < start)
                ):
                    raise ValueError((key, start, note, velocity, duration))
                events.extend([[start, note, velocity], [end, note, 0]])
            patch["sequence"] = {"lengthMs": length, "events": events}
        result[key] = FactoryEntry(bank, item["display"], item["play"], patch, basic, knobs)
    return result


def panel_banks(catalog):
    start = 0
    entries = list(catalog.values())
    for label, groups in PANEL_BANKS.items():
        size = sum(entry.bank in groups for entry in entries)
        if not size and groups == ("usb_effects",):
            continue
        if not 1 <= size <= 16 or any(entry.bank not in groups for entry in entries[start:start + size]):
            raise ValueError((label, size, "panel bank layout"))
        yield label, start, size
        start += size
    if start != len(catalog):
        raise ValueError("incomplete panel bank layout")


def write_native(bank, destination, *, felucca=False):
    lines = ["/* Generated by tools/build_presets.py. */"]
    for i, entry in enumerate(bank.values()):
        if "sequence" in entry.patch:
            events = entry.patch["sequence"]["events"]
            data = ", ".join("{" + ", ".join(map(str, event)) + "}" for event in events)
            lines.append(f"static const KsTimedEvent phrase_{i}[] = {{ {data} }};")
    for i, entry in enumerate(bank.values()):
        routes = [*entry.patch["pins"], *([16 + ch, col, kind] for ch, col, kind in entry.patch.get("usbRoutes", []))]
        pins = ", ".join(f"0x{((kind - 1) << 9 | row << 4 | col):03x}"
                         for row, col, kind in routes)

        if pins:
            lines.append(f"static const FactoryPin pins_{i}[] = {{ {pins} }};")
        if entry.knobs:
            ids = ", ".join(f"M4_{enum}" for enum, _ in entry.knobs)
            values = ", ".join(str(value) for _, value in entry.knobs)
            lines.append(f"static const uint8_t control_ids_{i}[] = {{ {ids} }};")
            lines.append(f"static const ZiviReal control_values_{i}[] = {{ {values} }};")
    if felucca:
        lines.append("static const FactoryBank factory_banks[] = {")
        lines.extend(f'    {{ "{label}", {start}, {size} }},' for label, start, size in panel_banks(bank))
        lines.append("};")
    lines.append("static const FactoryPreset factory_presets[] = {")
    for i, (key, entry) in enumerate(bank.items()):
        patch = entry.patch
        pin_count = len(patch["pins"]) + len(patch.get("usbRoutes", []))
        pins = f"pins_{i}" if pin_count else "NULL"
        params = f"control_ids_{i}, control_values_{i}" if entry.knobs else "NULL, NULL"
        values = ", ".join(str(v) for v in entry.basic.values())
        phrase = f"phrase_{i}, {patch['sequence']['lengthMs']}, {len(patch['sequence']['events'])}" if "sequence" in patch else "NULL, 0, 0"
        full = sum(1 << col for col, amount in patch["ksRoutes"] if amount == 1)
        half = sum(1 << col for col, amount in patch["ksRoutes"] if amount == 2)
        lines.extend([f'    {{ "{key}", "{entry.display}", {{ {values} }},',
                      f"      {pins}, {params}, {pin_count}, {len(entry.knobs)}, "
                      f"{str(patch['ksMatrix']).lower()}, {phrase}, 0x{full:04x}, 0x{half:04x} }},"])
    lines.append("};")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path)
    parser.add_argument("--json-dir", type=Path)
    catalog = parser.add_mutually_exclusive_group()
    catalog.add_argument("--felucca", action="store_true")
    catalog.add_argument("--usb-effects", action="store_true")
    args = parser.parse_args()
    bank = load(felucca=args.felucca, usb_effects=args.usb_effects)
    if args.native:
        native = {**bank, **load(usb_effects=True)} if args.felucca else bank
        write_native(native, args.native, felucca=args.felucca)
    if args.json_dir:
        args.json_dir.mkdir(parents=True, exist_ok=True)
        for key, entry in bank.items():
            (args.json_dir / f"{key}.json").write_text(
                json.dumps(entry.patch, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
