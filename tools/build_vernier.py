"""Read the shared dial anchors and emit precomputed firmware segments."""
import json
from math import log2
from pathlib import Path
import re
import sys

DEFINITION = Path(__file__).resolve().parents[1] / "platform/smk37_fm1_base/vernier.def"


def load():
    curves = {}
    for line in DEFINITION.read_text().splitlines():
        if not line or line.startswith("//"):
            continue
        taper = re.fullmatch(r"TAPER\((\w+), ([\d.]+)\)", line)
        if taper:
            name, middle = taper.groups()
            curves[name] = float(middle)
            continue
        name, points = re.fullmatch(r"VERNIER\((\w+), (.*)\)", line).groups()
        pairs = json.loads("[" + points.replace("{", "[").replace("}", "]") + "]")
        if pairs[0][0] != 0 or pairs[-1][0] != 1 or pairs[0][1] <= 0:
            raise ValueError(f"Invalid endpoints for {name}")
        if any(b[0] <= a[0] or b[1] <= a[1] for a, b in zip(pairs, pairs[1:])):
            raise ValueError(f"Non-monotonic anchors for {name}")
        curves[name] = pairs
    return curves


def generate():
    lines = ["/* Generated from vernier.def by tools/build_vernier.py. */"]
    for name, points in load().items():
        if isinstance(points, float):
            lines.append(f"static const float vernier_{name}_middle = {points:.12g}f;")
            continue
        lines.append(f"static const VernierSegment vernier_{name}[] = {{")
        for (lo, hz), (hi, upper) in zip(points, points[1:]):
            slope = log2(upper / hz) / (hi - lo)
            fields = [lo, hi, hz, upper, log2(hz), slope, 1 / slope]
            values = ", ".join(f"{float(value):#.12g}f" for value in fields)
            lines.append(f"    {{{values}}},")
        lines.append("};")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    Path(sys.argv[1]).write_text(generate())
