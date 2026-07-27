# Find CHUD-related strings in a title DLL and the code that references them.
import re, sys, struct

def load(path):
    data = open(path, "rb").read()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    optsz = struct.unpack_from("<H", data, pe + 20)[0]
    magic = struct.unpack_from("<H", data, pe + 24)[0]
    imgbase = struct.unpack_from("<Q" if magic == 0x20b else "<I", data,
                                 pe + 24 + (24 if magic == 0x20b else 28))[0]
    secs = []
    off = pe + 24 + optsz
    for i in range(nsec):
        b = off + 40 * i
        name = data[b:b+8].rstrip(b"\0").decode("latin1")
        vsz, va, rsz, ra = struct.unpack_from("<IIII", data, b + 8)
        secs.append((name, va, vsz, ra, rsz))
    return data, secs, imgbase

def f2rva(secs, foff):
    for name, va, vsz, ra, rsz in secs:
        if ra <= foff < ra + rsz:
            return va + (foff - ra), name
    return None, None

def rva2f(secs, rva):
    for name, va, vsz, ra, rsz in secs:
        if va <= rva < va + max(vsz, rsz):
            o = ra + (rva - va)
            return o if o < ra + rsz else None
    return None

def text_range(secs):
    for name, va, vsz, ra, rsz in secs:
        if name == ".text":
            return va, vsz, ra, rsz
    return None

if __name__ == "__main__":
    path = sys.argv[1]
    needle = sys.argv[2].encode()
    data, secs, imgbase = load(path)
    tva, tvsz, tra, trsz = text_range(secs)

    # Collect candidate strings.
    strs = []
    for m in re.finditer(re.escape(needle), data):
        s = m.start()
        while s > 0 and 0x20 <= data[s-1] < 0x7f:
            s -= 1
        e = m.start()
        while e < len(data) and 0x20 <= data[e] < 0x7f:
            e += 1
        if e >= len(data) or data[e] != 0:
            continue
        rva, sec = f2rva(secs, s)
        if rva is None or sec == ".text":
            continue
        strs.append((rva, data[s:e].decode("latin1")))
    strs = sorted(set(strs))
    print(f"{len(strs)} strings containing {needle!r}")

    # Build lea rip-relative xref index over .text.
    targets = {r: t for r, t in strs}
    hits = {}
    text = data[tra:tra+trsz]
    for m in re.finditer(rb"\x48\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]", text):
        o = m.start()
        if o + 7 > len(text):
            continue
        disp = struct.unpack_from("<i", text, o + 3)[0]
        tgt = tva + o + 7 + disp
        if tgt in targets:
            hits.setdefault(tgt, []).append(tva + o)

    for rva, s in strs:
        xr = hits.get(rva, [])
        mark = "  <== " + ", ".join(f"0x{x:X}" for x in xr[:8]) if xr else ""
        print(f"  0x{rva:X}  {s[:90]!r}{mark}")
