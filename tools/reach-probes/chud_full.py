# READ-ONLY comprehensive CHUD diagnostic.
# One run, while the player moves the weapon around, produces:
#   1. the named widget alpha/fade/duration table
#   2. every float field in each record that MOVES while the gun moves
#      (that is what identifies the world-anchored markers riding the weapon)
#   3. what the 16 "records" actually are
#   4. candidate crosshair visibility state beyond the alpha fields
import struct, time, sys
import chud_live as L

STRIDE = 0xC60
ALPHA_BASE = 0x32C
NAMES = {0: "(unnamed 0)", 1: "weapon stats", 2: "CROSSHAIR", 3: "shield",
         4: "grenades", 5: "messages", 6: "motion sensor",
         7: "chapter title", 8: "cinematics"}
SECONDS = float(sys.argv[1]) if len(sys.argv) > 1 else 18.0

def chud_targets(mem, base, size):
    tls_va, err = L.resolve_tls_index(mem, base, size)
    if not tls_va:
        raise SystemExit("resolve failed: " + str(err))
    idx = mem.u32(tls_va)
    out = []
    for tid in L.threads_of(mem_pid):
        teb = L.teb_of(tid)
        if not teb:
            continue
        tp = mem.u64(teb + 0x58)
        if not tp:
            continue
        slot = mem.u64(tp + idx * 8)
        if not slot:
            continue
        chud = mem.u64(slot + 0x5B0)
        if chud and 0x10000 < chud < (1 << 47):
            out.append((tid, chud))
    return idx, out

if __name__ == "__main__":
    mem_pid = L.find_pid()
    if not mem_pid:
        raise SystemExit("MCC not running")
    base, size = L.find_module(mem_pid, L.DLL)
    if not base:
        raise SystemExit("haloreach.dll not loaded")
    mem = L.Mem(mem_pid)
    idx, targets = chud_targets(mem, base, size)
    print("pid %d  base 0x%X  tls idx %d  chud threads %d"
          % (mem_pid, base, idx, len(targets)))
    for tid, chud in targets:
        print("   thread %6d -> chud_globals 0x%X" % (tid, chud))
    tid, chud = targets[0]

    # ---------- 1. named widget table ----------
    print("\n=== 1. named widget state (thread %d) ===" % tid)
    print(" idx  widget            alpha    fadeTo   fadeDur")
    for i in range(9):
        a = mem.f32(chud + ALPHA_BASE + i * 4)
        t = mem.f32(chud + 0x350 + i * 4)
        d = mem.u32(chud + 0x374 + i * 4)
        print("  %d   %-16s %-8.3f %-8.3f %s"
              % (i, NAMES.get(i, "?"), a if a is not None else -1,
                 t if t is not None else -1, d))

    # ---------- 2. what moves while the gun moves ----------
    print("\n=== 2. sampling %.0fs - MOVE YOUR GUN AROUND NOW ===" % SECONDS)
    WINDOW = 0x2000   # cover well past one record
    first = mem.read(chud, WINDOW)
    mins = {}
    maxs = {}
    samples = 0
    end = time.time() + SECONDS
    while time.time() < end:
        blob = mem.read(chud, WINDOW)
        if blob and len(blob) == WINDOW:
            samples += 1
            for off in range(0, WINDOW - 4, 4):
                v = struct.unpack_from("<f", blob, off)[0]
                if v != v or abs(v) > 1e12:      # NaN / junk
                    continue
                if off not in mins or v < mins[off]:
                    mins[off] = v
                if off not in maxs or v > maxs[off]:
                    maxs[off] = v
        time.sleep(0.02)
    print("samples: %d" % samples)

    moving = [(o, mins[o], maxs[o]) for o in mins
              if maxs[o] - mins[o] > 1e-4]
    moving.sort(key=lambda r: -(r[2] - r[1]))
    print("\nfloat fields that CHANGED while the gun moved: %d" % len(moving))
    print(" offset   record  in-rec   min          max          span")
    for off, lo, hi in moving[:40]:
        rec = off // STRIDE
        inr = off % STRIDE
        tag = ""
        if rec == 0 and ALPHA_BASE <= inr < ALPHA_BASE + 9 * 4:
            tag = "  <== ALPHA %s" % NAMES.get((inr - ALPHA_BASE) // 4, "?")
        print("  +0x%05X  %2d    +0x%04X  %-12.4f %-12.4f %-10.4f%s"
              % (off, rec, inr, lo, hi, hi - lo, tag))

    # ---------- 3. record identity ----------
    print("\n=== 3. what the 16 'records' are ===")
    for n in range(16):
        rec = mem.read(chud + n * STRIDE, 0x40)
        if not rec:
            continue
        ptrs = sum(1 for k in range(0, 0x40, 8)
                   if 0x10000 < struct.unpack_from("<Q", rec, k)[0] < (1 << 47))
        print("  rec %2d  head=%s  plausible-pointers=%d"
              % (n, rec[:16].hex(" "), ptrs))
