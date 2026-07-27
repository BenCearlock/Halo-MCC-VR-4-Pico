# READ-ONLY live probe: dump Halo Reach's CHUD widget records from the running
# game so each slot can be identified instead of guessed.
#
# Never writes to the process. Uses PROCESS_VM_READ only.
#   py -3 chud_live.py
import ctypes as C, struct, sys
from ctypes import wintypes as W

GAME = "MCC-Win64-Shipping.exe"
DLL = "haloreach.dll"
TH32CS_SNAPPROCESS = 0x2
TH32CS_SNAPMODULE = 0x8
TH32CS_SNAPMODULE32 = 0x10
TH32CS_SNAPTHREAD = 0x4
PROCESS_VM_READ = 0x10
PROCESS_QUERY_INFORMATION = 0x400
THREAD_QUERY_INFORMATION = 0x40

k32 = C.WinDLL("kernel32", use_last_error=True)
ntdll = C.WinDLL("ntdll")

class PROCESSENTRY32(C.Structure):
    _fields_ = [("dwSize", W.DWORD), ("cntUsage", W.DWORD),
                ("th32ProcessID", W.DWORD),
                ("th32DefaultHeapID", C.POINTER(C.c_ulong)),
                ("th32ModuleID", W.DWORD), ("cntThreads", W.DWORD),
                ("th32ParentProcessID", W.DWORD), ("pcPriClassBase", C.c_long),
                ("dwFlags", W.DWORD), ("szExeFile", C.c_char * 260)]

class MODULEENTRY32(C.Structure):
    _fields_ = [("dwSize", W.DWORD), ("th32ModuleID", W.DWORD),
                ("th32ProcessID", W.DWORD), ("GlblcntUsage", W.DWORD),
                ("ProccntUsage", W.DWORD), ("modBaseAddr", C.POINTER(C.c_byte)),
                ("modBaseSize", W.DWORD), ("hModule", W.HMODULE),
                ("szModule", C.c_char * 256), ("szExePath", C.c_char * 260)]

class THREADENTRY32(C.Structure):
    _fields_ = [("dwSize", W.DWORD), ("cntUsage", W.DWORD),
                ("th32ThreadID", W.DWORD), ("th32OwnerProcessID", W.DWORD),
                ("tpBasePri", C.c_long), ("tpDeltaPri", C.c_long),
                ("dwFlags", W.DWORD)]

class THREAD_BASIC_INFORMATION(C.Structure):
    _fields_ = [("ExitStatus", C.c_long), ("TebBaseAddress", C.c_void_p),
                ("UniqueProcessId", C.c_void_p), ("UniqueThreadId", C.c_void_p),
                ("AffinityMask", C.c_void_p), ("Priority", C.c_long),
                ("BasePriority", C.c_long)]

def find_pid():
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    e = PROCESSENTRY32(); e.dwSize = C.sizeof(e)
    ok = k32.Process32First(snap, C.byref(e))
    while ok:
        if e.szExeFile.decode(errors="ignore").lower() == GAME.lower():
            k32.CloseHandle(snap); return e.th32ProcessID
        ok = k32.Process32Next(snap, C.byref(e))
    k32.CloseHandle(snap); return None

def find_module(pid, name):
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)
    e = MODULEENTRY32(); e.dwSize = C.sizeof(e)
    ok = k32.Module32First(snap, C.byref(e))
    while ok:
        if e.szModule.decode(errors="ignore").lower() == name.lower():
            base = C.cast(e.modBaseAddr, C.c_void_p).value
            k32.CloseHandle(snap); return base, e.modBaseSize
        ok = k32.Module32Next(snap, C.byref(e))
    k32.CloseHandle(snap); return None, None

def threads_of(pid):
    out = []
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
    e = THREADENTRY32(); e.dwSize = C.sizeof(e)
    ok = k32.Thread32First(snap, C.byref(e))
    while ok:
        if e.th32OwnerProcessID == pid:
            out.append(e.th32ThreadID)
        ok = k32.Thread32Next(snap, C.byref(e))
    k32.CloseHandle(snap); return out

class Mem:
    def __init__(self, pid):
        self.h = k32.OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
                                 False, pid)
        if not self.h:
            raise SystemExit("OpenProcess failed %d" % C.get_last_error())
    def read(self, addr, n):
        buf = (C.c_char * n)(); got = C.c_size_t(0)
        if not k32.ReadProcessMemory(self.h, C.c_void_p(addr), buf, n,
                                     C.byref(got)):
            return None
        return bytes(buf[:got.value])
    def u32(self, a):
        b = self.read(a, 4); return struct.unpack("<I", b)[0] if b else None
    def u64(self, a):
        b = self.read(a, 8); return struct.unpack("<Q", b)[0] if b else None
    def f32(self, a):
        b = self.read(a, 4); return struct.unpack("<f", b)[0] if b else None

def teb_of(tid):
    h = k32.OpenThread(THREAD_QUERY_INFORMATION, False, tid)
    if not h:
        return None
    tbi = THREAD_BASIC_INFORMATION()
    st = ntdll.NtQueryInformationThread(h, 0, C.byref(tbi), C.sizeof(tbi), None)
    k32.CloseHandle(h)
    return tbi.TebBaseAddress if st == 0 else None

# ---- resolve the CHUD TLS index the same way the mod does -----------------
def resolve_tls_index(mem, base, size):
    name = b"chud_show_crosshair\x00"
    CHUNK = 1 << 20
    name_va = None
    off = 0
    while off < size:
        blob = mem.read(base + off, min(CHUNK, size - off))
        if blob:
            i = blob.find(name)
            if i >= 0:
                name_va = base + off + i
                break
        off += CHUNK - len(name)
    if not name_va:
        return None, "name string not found"
    needle = struct.pack("<Q", name_va)
    entry = None
    off = 0
    while off < size:
        blob = mem.read(base + off, min(CHUNK, size - off))
        if blob:
            i = blob.find(needle)
            if i >= 0 and (off + i) % 8 == 0:
                entry = base + off + i
                break
        off += CHUNK - 8
    if not entry:
        return None, "script-table entry not found"
    impl = mem.u64(entry + 0x18)
    if not impl or not (base <= impl < base + size):
        return None, "implementation pointer out of module"
    fn = mem.read(impl, 0x40)
    if not fn or fn[0x30] != 0x8B or fn[0x31] != 0x0D:
        return None, "TLS-index instruction not where expected"
    rel = struct.unpack_from("<i", fn, 0x32)[0]
    return impl + 0x36 + rel, None

if __name__ == "__main__":
    pid = find_pid()
    if not pid:
        raise SystemExit("MCC is not running")
    base, size = find_module(pid, DLL)
    if not base:
        raise SystemExit("haloreach.dll not loaded (load a Reach level first)")
    mem = Mem(pid)
    print("pid %d  haloreach.dll base 0x%X size 0x%X" % (pid, base, size))

    tls_va, err = resolve_tls_index(mem, base, size)
    if not tls_va:
        raise SystemExit("could not resolve: " + err)
    idx = mem.u32(tls_va)
    print("CHUD TLS index global at +0x%X  value=%s" % (tls_va - base, idx))

    # Find every thread whose TLS slot yields a plausible chud_globals.
    found = []
    for tid in threads_of(pid):
        teb = teb_of(tid)
        if not teb:
            continue
        tls_ptr = mem.u64(teb + 0x58)
        if not tls_ptr:
            continue
        slot = mem.u64(tls_ptr + idx * 8)
        if not slot:
            continue
        chud = mem.u64(slot + 0x5B0)
        if chud and 0x10000 < chud < (1 << 47):
            found.append((tid, chud))
    print("threads with a CHUD globals pointer: %d" % len(found))
    if not found:
        raise SystemExit("no thread had one - are you in a level?")

    STRIDE = 0xC60
    for tid, chud in found[:2]:
        print("\n=== thread %d  chud_globals 0x%X ===" % (tid, chud))
        print(" slot |    alpha(+334) fadeTo(+358) fadeMs(+37C) | "
              "first 0x10 bytes of the record")
        for n in range(16):
            rec = chud + n * STRIDE
            a = mem.f32(rec + 0x334)
            ft = mem.f32(rec + 0x358)
            fm = mem.u32(rec + 0x37C)
            head = mem.read(rec, 0x10)
            print("  %2d  | %12s %12s %12s | %s" % (
                n,
                "%.3f" % a if a is not None else "??",
                "%.3f" % ft if ft is not None else "??",
                str(fm) if fm is not None else "??",
                head.hex(" ") if head else "??"))
