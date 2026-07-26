# READ-ONLY: watch the CHUD per-widget flag/alpha arrays and report which
# indices actually move while the player acts. Identifies the crosshair slot
# by observation instead of assumption.
import struct, time, sys
import chud_live as L

FLAGS_OFF = 0x320
FLAGS_N = 12
ALPHA_OFF = 0x32C
ALPHA_N = 20
SECONDS = float(sys.argv[1]) if len(sys.argv) > 1 else 20.0

if __name__ == "__main__":
    pid = L.find_pid()
    base, size = L.find_module(pid, L.DLL)
    mem = L.Mem(pid)
    tls_va, _ = L.resolve_tls_index(mem, base, size)
    idx = mem.u32(tls_va)

    targets = []
    for tid in L.threads_of(pid):
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
            targets.append((tid, chud))
    if not targets:
        raise SystemExit("no CHUD globals - are you in a level?")

    tid, chud = targets[0]
    print("watching thread %d chud_globals 0x%X for %.0fs" % (tid, chud, SECONDS))
    print("act now: zoom in/out, fire, swap weapons\n")

    seen_alpha = [set() for _ in range(ALPHA_N)]
    seen_flag = [set() for _ in range(FLAGS_N)]
    samples = 0
    end = time.time() + SECONDS
    while time.time() < end:
        blob = mem.read(chud + FLAGS_OFF, (ALPHA_OFF - FLAGS_OFF) + ALPHA_N * 4)
        if blob:
            samples += 1
            for i in range(FLAGS_N):
                seen_flag[i].add(blob[i])
            ab = blob[ALPHA_OFF - FLAGS_OFF:]
            for i in range(ALPHA_N):
                v = struct.unpack_from("<f", ab, i * 4)[0]
                seen_alpha[i].add(round(v, 3))
        time.sleep(0.03)

    print("samples: %d\n" % samples)
    print("FLAG bytes at +0x%03X" % FLAGS_OFF)
    for i in range(FLAGS_N):
        vals = sorted(seen_flag[i])
        mark = "   <== CHANGES" if len(vals) > 1 else ""
        print("  flag[%2d] +0x%03X  values=%s%s"
              % (i, FLAGS_OFF + i, vals, mark))

    print("\nALPHA floats at +0x%03X (crosshair fn writes +0x334 = index %d)"
          % (ALPHA_OFF, (0x334 - ALPHA_OFF) // 4))
    for i in range(ALPHA_N):
        vals = sorted(seen_alpha[i])
        mark = "   <== CHANGES" if len(vals) > 1 else ""
        star = "  *crosshair-fn target*" if ALPHA_OFF + i * 4 == 0x334 else ""
        shown = vals if len(vals) <= 6 else [vals[0], "...", vals[-1]]
        print("  alpha[%2d] +0x%03X  values=%s%s%s"
              % (i, ALPHA_OFF + i * 4, shown, mark, star))
