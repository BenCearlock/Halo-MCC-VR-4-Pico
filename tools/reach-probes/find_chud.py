# Locate a chud_draw_widget homolog by Halo 3's structural fingerprint:
#   movsx r64, byte [desc+3]  and  movsx r32, byte [desc+4]   (same base reg)
#   followed by a call, then a class-2 compare against word [reg+4].
import sys, struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_64, x86
import petool as P
import rdis

def analyze(path, label):
    data, secs, ib = P.load(path)
    pd = rdis.pdata(data, secs)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    tva, tvsz, tra, trsz = [(va, vsz, ra, rsz)
                            for n, va, vsz, ra, rsz in secs if n == ".text"][0]
    print(f"=== {label}: {len(pd)} runtime functions ===")

    hits = []
    for (b, e, u) in pd:
        if e <= b or e - b > 0x1000 or b < tva or e > tva + tvsz:
            continue
        fo = P.rva2f(secs, b)
        if fo is None:
            continue
        code = data[fo:fo + (e - b)]
        plus3 = {}   # base reg -> addr
        plus4 = {}
        cls2 = []
        for ins in md.disasm(code, b):
            if ins.mnemonic in ("movsx", "movzx") and len(ins.operands) == 2:
                op = ins.operands[1]
                if op.type == x86.X86_OP_MEM and op.mem.index == 0:
                    if op.mem.disp == 3:
                        plus3.setdefault(op.mem.base, ins.address)
                    elif op.mem.disp == 4:
                        plus4.setdefault(op.mem.base, ins.address)
            if ins.mnemonic == "cmp" and len(ins.operands) == 2:
                a, c = ins.operands
                # cmp ax, word[reg+4]      /  cmp word[reg+4], 2
                if (a.type == x86.X86_OP_REG and c.type == x86.X86_OP_MEM
                        and c.mem.disp == 4 and ins.op_str.startswith(("ax", "cx", "dx", "bx"))):
                    cls2.append(ins.address)
                elif (a.type == x86.X86_OP_MEM and a.mem.disp == 4
                      and c.type == x86.X86_OP_IMM and c.imm == 2):
                    cls2.append(ins.address)
        shared = set(plus3) & set(plus4)
        if shared and cls2:
            hits.append((b, e, "PAIR+CLS2", sorted(shared), cls2))
        elif shared:
            hits.append((b, e, "PAIR", sorted(shared), []))

    strong = [h for h in hits if h[2] == "PAIR+CLS2"]
    print(f"functions with +3/+4 movsx pair: {len(hits)}")
    print(f"  ... AND a class-2 style compare: {len(strong)}")
    for b, e, kind, shared, cls2 in strong:
        print(f"  *** 0x{b:X}-0x{e:X} size 0x{e-b:X}  cls2@{[hex(c) for c in cls2]}")
    if len(hits) <= 40:
        for b, e, kind, shared, cls2 in hits:
            print(f"      {kind:10s} 0x{b:X}-0x{e:X} size 0x{e-b:X}")
    return strong

if __name__ == "__main__":
    MCC = "N:/SteamLibrary/steamapps/common/Halo The Master Chief Collection"
    analyze(f"{MCC}/halo3/halo3.dll", "halo3.dll (control)")
    print()
    analyze(f"{MCC}/haloreach/haloreach.dll", "haloreach.dll")
