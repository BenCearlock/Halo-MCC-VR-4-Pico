# READ-ONLY: find Reach's navpoint array live, using the structure HREK gave us
# rather than byte-matching the shipping compile.
#
# HREK (chud_navpoints.cpp, wrapper at reach_tag_test 0x8BA510):
#   block  = *(TLS[tls_index] + 0x8C0)         <- offset may differ in retail
#   array  = block + 0x604C + user*0x10D68     <- offsets may differ in retail
#   entry  : stride 0x88, position_worldspace at +0x3C (x,y,z floats)
#   count  : 20
import struct, time, sys
import chud_live as L

STRIDE = 0x88
POS = 0x3C
COUNT = 20

def finite(v):
    return v == v and abs(v) < 1e9

def plausible(x, y, z):
    if not (finite(x) and finite(y) and finite(z)):
        return False
    if x == 0.0 and y == 0.0 and z == 0.0:
        return False
    m = max(abs(x), abs(y), abs(z))
    return 0.01 < m < 20000.0

def scan_block(mem, addr, length):
    blob = mem.read(addr, length)
    if not blob:
        return []
    out = []
    limit = length - COUNT * STRIDE
    for off in range(0, max(0, limit), 4):
        good = 0
        for n in range(COUNT):
            o = off + n * STRIDE + POS
            x, y, z = struct.unpack_from("<fff", blob, o)
            if plausible(x, y, z):
                good += 1
        if good >= 4:
            out.append((good, off))
    out.sort(reverse=True)
    return out

if __name__ == "__main__":
    pid = L.find_pid()
    base, size = L.find_module(pid, L.DLL)
    mem = L.Mem(pid)
    tls_va, err = L.resolve_tls_index(mem, base, size)
    idx = mem.u32(tls_va)
    print("tls index %d" % idx)

    seen = set()
    results = []
    for tid in L.threads_of(pid):
        teb = L.teb_of(tid)
        if not teb:
            continue
        tp = mem.u64(teb + 0x58)
        if not tp:
            continue
        tls_block = mem.u64(tp + idx * 8)
        if not tls_block or tls_block in seen:
            continue
        seen.add(tls_block)
        # Walk the TLS block's pointer slots and scan each pointed-to region.
        for slot_off in range(0, 0x1200, 8):
            ptr = mem.u64(tls_block + slot_off)
            if not ptr or not (0x10000 < ptr < (1 << 47)):
                continue
            if ptr in seen:
                continue
            seen.add(ptr)
            found = scan_block(mem, ptr, 0x8000)
            for good, off in found[:1]:
                results.append((good, tid, tls_block, slot_off, ptr, off))

    results.sort(reverse=True)
    print("\ncandidate navpoint arrays: %d" % len(results))
    for good, tid, blk, slot_off, ptr, off in results[:6]:
        print("\n=== tid %d  tls+0x%03X -> 0x%X  array +0x%04X  (%d/%d) ==="
              % (tid, slot_off, ptr, off, good, COUNT))
        b = mem.read(ptr + off, COUNT * STRIDE)
        for n in range(COUNT):
            x, y, z = struct.unpack_from("<fff", b, n * STRIDE + POS)
            if plausible(x, y, z):
                print("   navpoint %2d  world = (%9.2f, %9.2f, %9.2f)"
                      % (n, x, y, z))
    if not results:
        print("none found - the array may be a plain global, not TLS-reached")
