# -*- coding: utf-8 -*-
"""
mem_dump.py — 從執行中的 32-bit process dump 出主模組的 in-memory image。
用途: 對加殼程式 (殼已在記憶體中自解密) 取得解密後的 code 以做 AOB scan / 還原分析。

用法:
    python mem_dump.py                # 自動依名稱關鍵字找 process
    python mem_dump.py --pid 1234     # 指定 PID
    python mem_dump.py --name 云荒     # 指定名稱關鍵字
    python mem_dump.py --list         # 只列出目前所有 process

輸出:
    <out>.raw.bin      : 記憶體對齊 (offset == RVA) 的整塊 image, 給 AOB scan 用
    <out>.rebuilt.exe  : section header 改成 PointerToRawData=VirtualAddress 的 dump,
                         可直接丟進 IDA / x32dbg / pe-bear 觀察 (IAT 未修, 不保證能跑)
"""
import sys, ctypes, struct, argparse
from ctypes import wintypes

k32 = ctypes.WinDLL('kernel32', use_last_error=True)

TH32CS_SNAPPROCESS = 0x00000002
TH32CS_SNAPMODULE   = 0x00000008
TH32CS_SNAPMODULE32 = 0x00000010
PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ           = 0x0010
MEM_COMMIT = 0x1000

class PROCESSENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wintypes.DWORD),("cntUsage", wintypes.DWORD),
        ("th32ProcessID", wintypes.DWORD),("th32DefaultHeapID", ctypes.POINTER(ctypes.c_ulong)),
        ("th32ModuleID", wintypes.DWORD),("cntThreads", wintypes.DWORD),
        ("th32ParentProcessID", wintypes.DWORD),("pcPriClassBase", ctypes.c_long),
        ("dwFlags", wintypes.DWORD),("szExeFile", ctypes.c_char * 260)]

class MODULEENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wintypes.DWORD),("th32ModuleID", wintypes.DWORD),
        ("th32ProcessID", wintypes.DWORD),("GlblcntUsage", wintypes.DWORD),
        ("ProccntUsage", wintypes.DWORD),("modBaseAddr", ctypes.POINTER(ctypes.c_byte)),
        ("modBaseSize", wintypes.DWORD),("hModule", wintypes.HMODULE),
        ("szModule", ctypes.c_char * 256),("szExePath", ctypes.c_char * 260)]

class MEMORY_BASIC_INFORMATION(ctypes.Structure):
    _fields_ = [("BaseAddress", ctypes.c_void_p),("AllocationBase", ctypes.c_void_p),
        ("AllocationProtect", wintypes.DWORD),("RegionSize", ctypes.c_size_t),
        ("State", wintypes.DWORD),("Protect", wintypes.DWORD),("Type", wintypes.DWORD)]

def list_processes():
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    pe = PROCESSENTRY32(); pe.dwSize = ctypes.sizeof(PROCESSENTRY32)
    out=[]
    if k32.Process32First(snap, ctypes.byref(pe)):
        while True:
            try: nm = pe.szExeFile.decode('mbcs', 'replace')
            except Exception: nm = repr(pe.szExeFile)
            out.append((pe.th32ProcessID, nm))
            if not k32.Process32Next(snap, ctypes.byref(pe)): break
    k32.CloseHandle(snap)
    return out

def main_module(pid):
    """回傳 (base, size, exepath) — 第一個 module 即 exe 本體。"""
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)
    if snap == -1: raise OSError("snapshot module fail err=%d"%ctypes.get_last_error())
    me = MODULEENTRY32(); me.dwSize = ctypes.sizeof(MODULEENTRY32)
    base=size=0; path=''
    if k32.Module32First(snap, ctypes.byref(me)):
        base = ctypes.cast(me.modBaseAddr, ctypes.c_void_p).value
        size = me.modBaseSize
        path = me.szExePath.decode('mbcs','replace')
    k32.CloseHandle(snap)
    return base, size, path

def dump(pid, out):
    base, size, path = main_module(pid)
    if not base: raise SystemExit("找不到主模組 (需與目標同 32/64 位? 用 32-bit python 跑此腳本)")
    print("pid=%d base=0x%X size=0x%X" % (pid, base, size))
    print("exe=%s" % path)

    h = k32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, pid)
    if not h: raise OSError("OpenProcess fail err=%d" % ctypes.get_last_error())

    buf = bytearray(size)            # 記憶體對齊: buf[rva] == 該 VA 的 byte
    mbi = MEMORY_BASIC_INFORMATION()
    addr = base; end = base + size
    read_bytes = ctypes.c_size_t(0)
    committed = 0; regions = 0
    while addr < end:
        r = k32.VirtualQueryEx(h, ctypes.c_void_p(addr), ctypes.byref(mbi), ctypes.sizeof(mbi))
        if r == 0: break
        rsize = mbi.RegionSize
        rbase = mbi.BaseAddress or addr
        if mbi.State == MEM_COMMIT:
            to_read = min(rsize, end - rbase)
            tmp = (ctypes.c_char * to_read)()
            ok = k32.ReadProcessMemory(h, ctypes.c_void_p(rbase), tmp, to_read, ctypes.byref(read_bytes))
            n = read_bytes.value
            if ok or n:
                off = rbase - base
                buf[off:off+n] = bytes(tmp[:n])
                committed += n; regions += 1
        addr = rbase + rsize
    k32.CloseHandle(h)
    print("讀到 committed=0x%X bytes over %d regions" % (committed, regions))

    rawpath = out + ".raw.bin"
    with open(rawpath, "wb") as f: f.write(buf)
    print("[+] 寫出 %s (%d bytes, offset==RVA)" % (rawpath, len(buf)))

    # rebuild: 把 section header 的 raw 指向 virtual, 讓分析工具能正確讀
    try:
        rebuilt = bytearray(buf)
        e = struct.unpack_from('<I', rebuilt, 0x3C)[0]
        if rebuilt[e:e+4] == b'PE\x00\x00':
            nsec = struct.unpack_from('<H', rebuilt, e+6)[0]
            szopt = struct.unpack_from('<H', rebuilt, e+20)[0]
            st = e + 24 + szopt
            for i in range(nsec):
                o = st + i*40
                va = struct.unpack_from('<I', rebuilt, o+12)[0]
                vs = struct.unpack_from('<I', rebuilt, o+8)[0]
                struct.pack_into('<I', rebuilt, o+16, vs)   # SizeOfRawData = VirtualSize
                struct.pack_into('<I', rebuilt, o+20, va)   # PointerToRawData = VirtualAddress
            repath = out + ".rebuilt.exe"
            with open(repath, "wb") as f: f.write(rebuilt)
            print("[+] 寫出 %s (section raw=virtual, 給 IDA/x32dbg 看)" % repath)
    except Exception as ex:
        print("rebuild 略過:", ex)

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument('--pid', type=int)
    ap.add_argument('--name', default='云荒')
    ap.add_argument('--out', default='D:\\repos\\CaoCao_cht_patch\\tools\\yunhuang_dump')
    ap.add_argument('--list', action='store_true')
    a = ap.parse_args()

    if a.list:
        for pid, nm in sorted(list_processes()):
            print("%6d  %s" % (pid, nm))
        sys.exit(0)

    pid = a.pid
    if not pid:
        cands = [(p, n) for p, n in list_processes()
                 if a.name in n or '.exe' in n.lower() and a.name in n]
        cands = [(p, n) for p, n in list_processes() if a.name in n]
        if not cands:
            print("找不到名稱含 %r 的 process。用 --list 看清單, 或 --pid 指定。" % a.name)
            sys.exit(1)
        if len(cands) > 1:
            print("多個候選, 請用 --pid 指定:")
            for p, n in cands: print("  %6d  %s" % (p, n))
            sys.exit(1)
        pid, nm = cands[0]
        print("命中 process: pid=%d name=%s" % (pid, nm))

    dump(pid, a.out)
