#!/usr/bin/env python3
"""Build the interpolation tables used by the 32-bit SMK audio profile."""
import math
from pathlib import Path
import sys

SIZE = 512


def filter_gain(unit):
    value = math.tan(math.pi * .22 * unit)
    return value / (1 + value)


def table(name, func, last=SIZE):
    values = [f"{func(i / SIZE):.9e}f" for i in range(last + 1)]
    # An inclusive endpoint plus a repeated guard sample for rounding at 1.0.
    values.append(values[-1])
    rows = ("    " + ", ".join(values[i:i + 6]) + "," for i in range(0, len(values), 6))
    return f"static const float {name}[] = {{\n" + "\n".join(rows) + "\n};\n"


path = Path(sys.argv[1])
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(
    f"enum {{ DSP_TABLE_SIZE = {SIZE} }};\n"
    + table("ZIVI_EXP2_TABLE", lambda x: 2 ** x)
    + table("ZIVI_SINE_TABLE", lambda x: math.sin(math.tau * x))
    + table("ZIVI_FILTER_TABLE", filter_gain, math.ceil(SIZE * .25 / .22))
)
