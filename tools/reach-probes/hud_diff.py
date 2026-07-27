# READ-ONLY live memory-diff probe: find what ACTUALLY changes when hud_size
# is moved in the F1 menu, instead of assuming it is the chud_globals
# curvature-info safe frame (proven correct in shape/write/persistence, but
# with zero visible effect for Reach - see docs/RE-notes.md).
#
# This is the same method that originally found Halo 3's working safe-frame
# anchor: snapshot memory, change the slider, snapshot again, and look at what
# is actually different. It makes no assumption about which struct or field
# owns the value.
#
# Never writes to the process. Uses PROCESS_VM_READ only.
#
# Usage:
#   1. py -3 hud_diff.py snap baseline.bin      (while HUD looks normal)
#   2. move hud_size in the F1 menu by a LARGE amount (e.g. 0.87 -> 0.30)
#   3. py -3 hud_diff.py snap after.bin
#   4. py -3 hud_diff.py diff baseline.bin after.bin
#
# diff reports every 4-byte-aligned float that changed between the two
# snapshots and is still a plausible 0.0-2.0 scale-like value, grouped by
# region, so a real HUD-scale field stands out from render-camera/animation
# noise instead of being lost in it.
import ctypes as C, struct, sys, pickle
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


def scan_regions(h):
    """Yield (base, protect, type, data) for every committed private/mapped
    readable region, skipping huge (>64MB) regions to keep this tractable."""
    addr = 0x10000
    mbi = MEMORY_BASIC_INFORMATION64()
    while addr < 0x7FFFFFFF0000:
        if k32.VirtualQueryEx(h, C.c_void_p(addr), C.byref(mbi),
                              C.sizeof(mbi)) != C.sizeof(mbi):
            addr += 0x1000
            continue
        base, size = mbi.BaseAddress, mbi.RegionSize
        readable = (mbi.State == 0x1000 and
                    not (mbi.Protect & 0x101) and mbi.Protect != 0)
        interesting_type = mbi.Type in (0x20000, 0x40000)  # PRIVATE, MAPPED
        if readable and interesting_type and 0 < size <= (64 << 20):
            blob = read(h, base, size)
            if blob:
                yield (base, mbi.Protect, mbi.Type, blob)
        addr = base + size if size else addr + 0x1000


def do_snap(outpath):
    pid = find_pid()
    if not pid:
        print(f"{GAME} is not running.")
        return 1
    h = k32.OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, False, pid)
    if not h:
        print(f"OpenProcess failed: {C.get_last_error()}")
        return 1
    print(f"attached to pid {pid}, snapshotting all private+mapped RW/RO "
          f"regions <=64MB (this can take 30-90s)...")
    regions = list(scan_regions(h))
    total = sum(len(b) for _, _, _, b in regions)
    print(f"captured {len(regions)} region(s), {total/1e6:.1f} MB total")
    with open(outpath, "wb") as f:
        pickle.dump(regions, f, protocol=4)
    print(f"wrote {outpath}")
    k32.CloseHandle(h)
    return 0


def do_diff(path_a, path_b):
    with open(path_a, "rb") as f:
        regions_a = pickle.load(f)
    with open(path_b, "rb") as f:
        regions_b = pickle.load(f)
    b_by_base = {base: (prot, typ, blob) for base, prot, typ, blob in regions_b}

    print(f"comparing {len(regions_a)} baseline region(s) against "
          f"{len(regions_b)} after-region(s)...")
    total_changed_floats = 0
    reported = 0
    for base, prot, typ, blob_a in regions_a:
        entry = b_by_base.get(base)
        if not entry:
            continue
        _, _, blob_b = entry
        n = min(len(blob_a), len(blob_b))
        # 4-byte aligned float scan
        changes = []
        for off in range(0, n - 4, 4):
            wa = blob_a[off:off+4]
            wb = blob_b[off:off+4]
            if wa == wb:
                continue
            fa = struct.unpack("<f", wa)[0]
            fb = struct.unpack("<f", wb)[0]
            # plausible "scale-like" value range on BOTH sides
            def plausible(x):
                return -0.01 <= x <= 3.0 and (x == 0.0 or abs(x) > 1e-6)
            if plausible(fa) and plausible(fb) and abs(fa - fb) > 0.02:
                changes.append((off, fa, fb))
        total_changed_floats += len(changes)
        if changes:
            reported += 1
            typename = "MAPPED" if typ == 0x40000 else "PRIVATE"
            print(f"\nregion 0x{base:016X} ({typename}, protect 0x{prot:X}, "
                  f"{len(blob_a)} bytes): {len(changes)} plausible float "
                  f"change(s)")
            for off, fa, fb in changes[:40]:
                print(f"    +0x{off:06X}  {fa:.4f} -> {fb:.4f}")
            if len(changes) > 40:
                print(f"    ... and {len(changes)-40} more in this region")
    print(f"\ntotal: {reported} region(s) with plausible scale-like changes, "
          f"{total_changed_floats} float(s) total")
    return 0


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "snap":
        sys.exit(do_snap(sys.argv[2]))
    elif len(sys.argv) >= 4 and sys.argv[1] == "diff":
        sys.exit(do_diff(sys.argv[2], sys.argv[3]))
    else:
        print(__doc__)
        sys.exit(1)
