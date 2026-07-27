# READ-ONLY live probe: find EVERY copy of Halo: Reach's chud curvature record
# in the running game and report each one's live global-safe-frame pair.
#
# The mod's own scan only inspects private read-write memory, which is where
# Halo 3's tag data lives. This probe deliberately inspects every readable
# committed region so we can tell two things apart:
#
#   * the mod wrote the only copy and Reach still ignores it, or
#   * more copies exist and the mod wrote the wrong one.
#
# Never writes to the process. Uses PROCESS_VM_READ only.
#   py -3 safeframe_find.py            (one pass)
#   py -3 safeframe_find.py watch      (re-read the hits every 500 ms)
import ctypes as C, struct, sys, time
from ctypes import wintypes as W

GAME = "MCC-Win64-Shipping.exe"
PROCESS_VM_READ = 0x10
PROCESS_QUERY_INFORMATION = 0x400
TH32CS_SNAPPROCESS = 0x2

k32 = C.WinDLL("kernel32", use_last_error=True)


class PROCESSENTRY32(C.Structure):
    _fields_ = [("dwSize", W.DWORD), ("cntUsage", W.DWORD),
                ("th32ProcessID", W.DWORD),
                ("th32DefaultHeapID", C.POINTER(C.c_ulong)),
                ("th32ModuleID", W.DWORD), ("cntThreads", W.DWORD),
                ("th32ParentProcessID", W.DWORD), ("pcPriClassBase", C.c_long),
                ("dwFlags", W.DWORD), ("szExeFile", C.c_char * 260)]


class MEMORY_BASIC_INFORMATION64(C.Structure):
    _fields_ = [("BaseAddress", C.c_ulonglong),
                ("AllocationBase", C.c_ulonglong),
                ("AllocationProtect", W.DWORD), ("__alignment1", W.DWORD),
                ("RegionSize", C.c_ulonglong), ("State", W.DWORD),
                ("Protect", W.DWORD), ("Type", W.DWORD),
                ("__alignment2", W.DWORD)]


def find_pid():
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    e = PROCESSENTRY32()
    e.dwSize = C.sizeof(e)
    ok = k32.Process32First(snap, C.byref(e))
    while ok:
        if e.szExeFile.decode("latin1").lower() == GAME.lower():
            k32.CloseHandle(snap)
            return e.th32ProcessID
        ok = k32.Process32Next(snap, C.byref(e))
    k32.CloseHandle(snap)
    return None


def read(h, addr, size):
    buf = (C.c_char * size)()
    got = C.c_size_t(0)
    if not k32.ReadProcessMemory(h, C.c_void_p(addr), buf, size, C.byref(got)):
        return None
    return buf.raw[:got.value]


# Reach's record, from HREK: virtual width/height, sensor origin, sensor radius,
# vehicle 3d sensor radius, blip radius, four minimap points, then the safe
# frame pair 60 bytes in. Wildcards match the mod's adapter exactly.
ANCHOR = bytes.fromhex(
    "00050000" "d0020000"        # 1280, 720
    "0000b042" "00000000"        # 88.0, sensor origin Y (wild)
    "00000000" "0000a042"        # sensor radius (wild), 80.0
    "0000c040")                  # blip radius 6.0
MASK = bytes.fromhex(
    "ffffffff" "ffffffff"
    "ffffffff" "00000000"
    "00000000" "ffffffff"
    "ffffffff")
SAFE_FRAME_OFFSET = 60
DINGS_OFFSET = 68
RECORD_SPAN = 76

PROT = {0x01: "NOACCESS", 0x02: "R", 0x04: "RW", 0x08: "WC",
        0x10: "X", 0x20: "RX", 0x40: "RWX", 0x80: "WCX"}
TYPE = {0x20000: "PRIVATE", 0x40000: "MAPPED", 0x1000000: "IMAGE"}


def scan(h, progress=False):
    hits = []
    addr = 0x10000
    regions = 0
    t0 = time.time()
    mbi = MEMORY_BASIC_INFORMATION64()
    while addr < 0x7FFFFFFF0000:
        regions += 1
        if progress and regions % 20000 == 0:
            print(f"    ...scanned to 0x{addr:016X} "
                  f"({regions} regions, {time.time()-t0:.0f}s, "
                  f"{len(hits)} hit(s) so far)", file=sys.stderr)
        if k32.VirtualQueryEx(h, C.c_void_p(addr), C.byref(mbi),
                              C.sizeof(mbi)) != C.sizeof(mbi):
            addr += 0x1000
            continue
        base, size = mbi.BaseAddress, mbi.RegionSize
        readable = (mbi.State == 0x1000 and
                    not (mbi.Protect & 0x101) and mbi.Protect != 0)
        if readable and size and size < (1 << 32):
            blob = read(h, base, size)
            if blob:
                i = blob.find(ANCHOR[:8])
                while i != -1:
                    if i + RECORD_SPAN <= len(blob):
                        cand = blob[i:i + len(ANCHOR)]
                        if all((cand[k] & MASK[k]) == (ANCHOR[k] & MASK[k])
                               for k in range(len(ANCHOR))):
                            hits.append((base + i, mbi.Protect, mbi.Type,
                                         blob[i:i + RECORD_SPAN]))
                    i = blob.find(ANCHOR[:8], i + 1)
        addr = base + size if size else addr + 0x1000
    return hits


def show(record):
    h, v = struct.unpack_from("<ff", record, SAFE_FRAME_OFFSET)
    dh, dv = struct.unpack_from("<ff", record, DINGS_OFFSET)
    oy, sr = struct.unpack_from("<ff", record, 12)
    return (f"safe frame {h:.4f}/{v:.4f}  dings {dh:.2f}/{dv:.2f}  "
            f"(origin Y {oy:.0f}, sensor radius {sr:.0f})")


def main():
    pid = find_pid()
    if not pid:
        print(f"{GAME} is not running.")
        return 1
    h = k32.OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, False, pid)
    if not h:
        print(f"OpenProcess failed: {C.get_last_error()} (run as the same user)")
        return 1
    print(f"attached to pid {pid}, scanning every readable region "
          f"(this can take 30-90s)...")
    t0 = time.time()
    hits = scan(h, progress=True)
    print(f"scan finished in {time.time()-t0:.0f}s", file=sys.stderr)
    print(f"\n{len(hits)} Reach curvature record(s) found "
          f"(the mod's scan only looks at PRIVATE/RW):\n")
    for addr, prot, typ, rec in hits:
        p = PROT.get(prot & 0xFF, hex(prot))
        t = TYPE.get(typ, hex(typ))
        seen = "  <-- mod would scan this" if (t == "PRIVATE" and p == "RW") \
            else "  <-- MOD NEVER SCANS THIS REGION"
        print(f"  {addr:016X}  {t:8} {p:4}  {show(rec)}{seen}")
    if len(sys.argv) > 1 and sys.argv[1] == "watch":
        print("\nwatching (Ctrl+C to stop)...")
        try:
            while True:
                time.sleep(0.5)
                line = []
                for addr, prot, typ, _ in hits:
                    rec = read(h, addr, RECORD_SPAN)
                    if rec:
                        hv = struct.unpack_from("<ff", rec, SAFE_FRAME_OFFSET)
                        line.append(f"{hv[0]:.3f}/{hv[1]:.3f}")
                print("   " + "   ".join(line))
        except KeyboardInterrupt:
            pass
    elif len(sys.argv) > 1 and sys.argv[1] == "rescan":
        # Re-run the FULL region scan repeatedly (not just re-read the known
        # addresses). If Reach ever re-derives this record into a NEW memory
        # location - a fresh allocation on a resolution/HUD change, a
        # double-buffered copy, or a per-frame scratch build - a rescan finds
        # it appearing at a different address while the old one goes stale.
        # This is slow (full VM walk) so it only re-scans every ~2 seconds.
        print("\nrescanning the full address space every ~2s (Ctrl+C to stop)...")
        seen = {addr for addr, *_ in hits}
        try:
            while True:
                time.sleep(2.0)
                cur = scan(h)
                cur_addrs = {addr for addr, *_ in cur}
                newly = cur_addrs - seen
                gone = seen - cur_addrs
                if newly or gone:
                    print(f"  CHANGE: +{len(newly)} new, -{len(gone)} gone")
                    for addr, prot, typ, rec in cur:
                        if addr in newly:
                            t = TYPE.get(typ, hex(typ))
                            p = PROT.get(prot & 0xFF, hex(prot))
                            print(f"    NEW  {addr:016X}  {t:8} {p:4}  {show(rec)}")
                    for addr in gone:
                        print(f"    GONE {addr:016X}")
                    seen = cur_addrs
                else:
                    hv = [show(r)[11:24] for _, _, _, r in cur[:1]]
                    print(f"  no new/removed copies ({len(cur)} total)"
                          + (f"  slot0={hv[0]}" if hv else ""))
        except KeyboardInterrupt:
            pass
    k32.CloseHandle(h)
    return 0


if __name__ == "__main__":
    sys.exit(main())
