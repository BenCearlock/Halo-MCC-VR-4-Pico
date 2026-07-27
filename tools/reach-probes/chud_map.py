# Map CHUD alpha-array indices to widget names, using Reach's own script
# functions. Each chud_show_* / chud_fade_* implementation writes the alpha for
# the widget it controls, so its displacement names that index.
import re, struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_64, x86
import petool as P, rdis

R = 'N:/SteamLibrary/steamapps/common/Halo The Master Chief Collection/haloreach/haloreach.dll'
data, secs, ib = P.load(R)
pd = rdis.pdata(data, secs)
md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True

# Every chud_* script name in the image.
names = {}
for m in re.finditer(rb'chud_[a-z0-9_]{3,40}\x00', data):
    s = m.group(0)[:-1].decode()
    rva, sec = P.f2rva(secs, m.start())
    if sec and sec != '.text':
        names.setdefault(s, rva)

print("chud_* script-name strings: %d" % len(names))

def impl_of(name_rva):
    va = ib + name_rva
    needle = struct.pack('<Q', va)
    hits = [m.start() for m in re.finditer(re.escape(needle), data)]
    if len(hits) != 1:
        return None
    entry_rva = P.f2rva(secs, hits[0])[0]
    fo = P.rva2f(secs, entry_rva + 0x18)
    if fo is None:
        return None
    impl = struct.unpack_from('<Q', data, fo)[0]
    if not impl:
        return None
    r = impl - ib
    return r if 0 < r < 0x4EDA000 else None

# Which displacements does this function store floats/ints to, off the pointer
# it loads from TLS+0x5B0?
def written_offsets(rva):
    b, e = rdis.enclosing(pd, rva)
    if not b or e - b > 0x400:
        return []
    fo = P.rva2f(secs, b)
    out = []
    for ins in md.disasm(data[fo:fo + (e - b)], b):
        if ins.mnemonic in ("movss", "mov") and len(ins.operands) == 2:
            dst = ins.operands[0]
            if dst.type == x86.X86_OP_MEM and 0x300 <= dst.mem.disp <= 0x400:
                out.append(dst.mem.disp)
    return sorted(set(out))

ALPHA_BASE = 0x32C
rows = []
for name, nrva in sorted(names.items()):
    impl = impl_of(nrva)
    if impl is None:
        continue
    offs = written_offsets(impl)
    if not offs:
        continue
    rows.append((name, impl, offs))

print("\n%-36s %-10s %s" % ("script function", "impl", "writes -> alpha index"))
for name, impl, offs in rows:
    desc = ", ".join(
        "+0x%03X(idx %d)" % (o, (o - ALPHA_BASE) // 4)
        if (o - ALPHA_BASE) % 4 == 0 and o >= ALPHA_BASE else "+0x%03X" % o
        for o in offs)
    print("%-36s 0x%06X  %s" % (name, impl, desc))
