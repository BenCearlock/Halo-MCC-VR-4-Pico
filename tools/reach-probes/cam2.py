# READ-ONLY: do Reach's cameras agree with each other?
# If the world renders from one camera and the CHUD projects from another, the
# two will report different orientations. Just play normally while this runs.
# compact camera: pos +0x00, forward +0x0C, up +0x18 (src/common/reach_render_logic.h)
import struct, time, sys, math
import chud_live as L

ACTIVE_VIEW = 0x04E389A8
STACK_PTRS = 0x00C878A8
CAM_OWNER = 0x04E38A90
PV_CAM = 0x03B0
SEC = float(sys.argv[1]) if len(sys.argv) > 1 else 12.0

def v3(mem, a):
    b = mem.read(a, 12)
    if not b:
        return None
    v = struct.unpack("<fff", b)
    return v if all(c == c and abs(c) < 1e6 for c in v) else None

def angle(a, b):
    na = math.sqrt(sum(c * c for c in a))
    nb = math.sqrt(sum(c * c for c in b))
    if na < 1e-6 or nb < 1e-6:
        return 0.0
    d = sum(a[i] * b[i] for i in range(3)) / (na * nb)
    return math.degrees(math.acos(max(-1.0, min(1.0, d))))

pid = L.find_pid()
base, _ = L.find_module(pid, L.DLL)
mem = L.Mem(pid)
view = mem.u64(base + ACTIVE_VIEW)
owner = mem.u64(base + CAM_OWNER)

probes = []
if view:
    probes.append(("player_view+0x3B0", view + PV_CAM))
for i in range(4):
    p = mem.u64(base + STACK_PTRS + i * 8)
    if p and 0x10000 < p < (1 << 47):
        probes.append(("stack[%d]" % i, p))
if owner and (not view or owner != view + PV_CAM):
    probes.append(("cam_owner", owner))

print("capturing %.0fs - play normally" % SEC)
hist = {n: [] for n, _ in probes}
t0 = time.time()
while time.time() - t0 < SEC:
    for n, a in probes:
        f = v3(mem, a + 0x0C)
        if f:
            hist[n].append(f)
    time.sleep(0.04)

print("\nprobe                 samples   total turn   vs first probe")
ref = probes[0][0]
for n, _ in probes:
    h = hist[n]
    if len(h) < 2:
        print("  %-20s  %4d      (no data)" % (n, len(h)))
        continue
    turn = max(angle(h[0], x) for x in h)
    r = hist[ref]
    div = 0.0
    for i in range(min(len(h), len(r))):
        div = max(div, angle(h[i], r[i]))
    print("  %-20s  %4d      %7.2f deg   %7.2f deg" % (n, len(h), turn, div))
print("\nIf every probe shows the same turn and ~0 divergence, the engine has ONE")
print("camera and the navpoint bug is in the projection, not a second camera.")
