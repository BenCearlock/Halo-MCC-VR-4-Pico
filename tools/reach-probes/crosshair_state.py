# READ-ONLY: find Reach's crosshair colour/state by watching what TOGGLES while
# the player alternates aiming at an enemy (red) and away (normal).
#
# Anything that flips between exactly two stable values in time with the player
# is crosshair state. That locates the widget data, and from there the draw.
import struct, time, sys
from collections import defaultdict
import chud_live as L

SEC = float(sys.argv[1]) if len(sys.argv) > 1 else 24.0
WINDOW = 0x4000

pid = L.find_pid()
if not pid:
    raise SystemExit("MCC not running")
base, size = L.find_module(pid, L.DLL)
if not base:
    raise SystemExit("haloreach.dll not loaded")
mem = L.Mem(pid)
tls_va, err = L.resolve_tls_index(mem, base, size)
if not tls_va:
    raise SystemExit("resolve failed: %s" % err)
idx = mem.u32(tls_va)

targets = []
seen = set()
for tid in L.threads_of(pid):
    teb = L.teb_of(tid)
    if not teb:
        continue
    tp = mem.u64(teb + 0x58)
    if not tp:
        continue
    blk = mem.u64(tp + idx * 8)
    if not blk:
        continue
    chud = mem.u64(blk + 0x5B0)
    if chud and 0x10000 < chud < (1 << 47) and chud not in seen:
        seen.add(chud)
        targets.append((tid, chud))
if not targets:
    raise SystemExit("no chud globals - are you in a level?")
tid, chud = targets[0]
print("watching chud_globals 0x%X for %.0fs" % (chud, SEC))
print("ALTERNATE: aim at an enemy (crosshair red), then away. Repeat.\n")

# Record the set of distinct values seen at each 4-byte slot.
vals = defaultdict(set)
samples = 0
end = time.time() + SEC
while time.time() < end:
    b = mem.read(chud, WINDOW)
    if b and len(b) == WINDOW:
        samples += 1
        for off in range(0, WINDOW, 4):
            vals[off].add(b[off:off + 4])
    time.sleep(0.03)

print("samples: %d" % samples)
# Slots that settled on exactly 2-4 distinct values are state flags/colours.
cands = [(off, v) for off, v in vals.items() if 2 <= len(v) <= 4]
cands.sort(key=lambda r: len(r[1]))
print("slots with 2-4 distinct values: %d\n" % len(cands))
for off, v in cands[:40]:
    shown = []
    for raw in list(v)[:4]:
        f = struct.unpack("<f", raw)[0]
        i = struct.unpack("<I", raw)[0]
        shown.append("%.3f/0x%08X" % (f, i) if abs(f) < 1e6 else "0x%08X" % i)
    print("  +0x%04X  %s" % (off, "   ".join(shown)))
