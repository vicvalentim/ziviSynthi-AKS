"""Generate and run the full physical-pad regression in the fm1-base simulator."""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("simulator", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)

events = [
    "wait 280", "expect zpin 1", f"expect zled12 {0x985810}",
    "pad 12 down 110", "wait 50", "expect zpin 0", f"expect zled12 {0x164C80}", "pad 12 up",
    "spin ALGO 1", "pad 12 down 110", "wait 50", "expect zpin 2", f"expect zled12 {0x484848}", "pad 12 up",
    "tap HOME", "expect zbank 1", "expect zled14 0", "expect zled15 0",
]
base = 0
counts = (14, 16, 16, 16, 11, 16, 16, 16, 7)
for bank, count in enumerate(counts, 1):
    events.append(f"expect zbank {bank}")
    for pad in range(count):
        events.extend((f"pad {pad} down 110", "wait 50", f"expect zpreset {base + pad + 4}",
                       f"expect zcursor {pad + 1}", f"expect zled{pad} {0xA85C0C}", f"pad {pad} up"))
    if count < 16:
        for pad in range(count, 16):
            events.extend((f"pad {pad} down 110", "wait 50", f"expect zpreset {base + count + 3}", f"pad {pad} up"))
    if bank != len(counts):
        events.append("tap RIGHT")
    base += count

events.extend(("shot presets-4x4.png", "tap HOME", "expect zrow 12", "expect zcol 1",
               "tap FX", "expect zzoom 0", "spin K4 1", "expect zpreset 4", "expect zbank 1",
               "spin K4 -1", "expect zpreset 131", "expect zbank 9",
               "wait 60", "shot overview-k4.png",
               "midi B0 00 00", "midi C0 7F", "expect zpreset 131",
               "midi B0 00 06", "midi C0 00", "expect zpreset 77",
               "midi B0 00 07", "midi C0 05", "expect zpreset 98",
               "expect zksevents 16",
               "tap FX", "expect zzoom 1", "spin K4 1", "expect zpreset 98"))
row, col = 11, 0
for base_row in range(0, 16, 4):
    for base_col in range(0, 16, 4):
        events.extend((f"spin PRESET {base_row - row}", f"spin SELECT {base_col - col}"))
        for pad in range(16):
            row, col = base_row + pad // 4, base_col + pad % 4
            events.extend((f"pad {pad} down 110", f"expect zrow {row + 1}", f"expect zcol {col + 1}",
                           f"pad {pad} up"))
events.extend(("wait 50", "expect zzoom 1", "shot matriz-4x4.png", "expect zmiss 0",
               "expect store_writes 0"))
script = args.output / "pad-grid.txt"
script.write_text("\n".join(events) + "\n")
subprocess.run([str(args.simulator.resolve()), script, args.output], check=True)
print(f"{sum(counts)} presets / {len(counts)} bancos, 16 regiões / 256 coordenadas, LEDs e foco verificados.")
