#!/usr/bin/env python3
"""════════════════════════════════════════════════════════════════════════
DIVIDED HORIZON — PE deliverable verifier (Spec 17, 19)

A .exe that cannot start on a target machine is worse than no .exe, because
it reads as "done". This script refuses to let the build claim success unless
the produced binary is:
  • a valid PE32+ (x86-64) executable
  • subsystem 2 (GUI) so no console window appears behind the game
  • importing ONLY DLLs that ship with every supported Windows release
    (kernel32/user32/gdi32/opengl32/winmm/advapi32/shell32/msvcrt)
Any third-party import (a DirectX redist, a VC++ runtime side-by-side
assembly, a vendor driver DLL) would make the zip fail on a clean machine.
════════════════════════════════════════════════════════════════════════"""
import struct, sys, os

UNIVERSAL_DLLS = {
    "kernel32.dll", "user32.dll", "gdi32.dll", "advapi32.dll", "shell32.dll",
    "opengl32.dll", "winmm.dll", "msvcrt.dll", "ole32.dll", "oleaut32.dll",
    "ws2_32.dll", "version.dll", "shlwapi.dll", "comdlg32.dll", "comctl32.dll",
    "imm32.dll", "wininet.dll", "crypt32.dll",
}

def fail(msg):
    print(f"   ✗ PE VERIFY FAILED: {msg}")
    return 1

def main(path):
    if not os.path.isfile(path):
        return fail(f"{path} does not exist")
    data = open(path, "rb").read()
    if data[:2] != b"MZ":
        return fail("missing MZ DOS header — not a PE file at all")
    pe_off = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe_off:pe_off+4] != b"PE\0\0":
        return fail(f"bad PE signature at 0x{pe_off:X}")
    machine = struct.unpack_from("<H", data, pe_off+4)[0]
    if machine != 0x8664:
        return fail(f"machine 0x{machine:04X} is not AMD64 (0x8664)")
    opt_off = pe_off + 24
    magic = struct.unpack_from("<H", data, opt_off)[0]
    if magic != 0x20B:
        return fail(f"optional-header magic 0x{magic:04X} is not PE32+ (0x20B)")
    want_sub = 2
    if len(sys.argv) > 2:
        want_sub = int(sys.argv[2])
    subsystem = struct.unpack_from("<H", data, opt_off + 68)[0]
    if subsystem != want_sub:
        if want_sub == 2:
            return fail(f"subsystem {subsystem} — expected 2 (GUI); a console box "
                        "would flash behind the game window")
        return fail(f"subsystem {subsystem} — expected {want_sub}")
    n_sec = struct.unpack_from("<H", data, pe_off+6)[0]
    opt_sz = struct.unpack_from("<H", data, pe_off+20)[0]
    sec_off = opt_off + opt_sz

    # PE32+ optional header: NumberOfRvaAndSizes at +108, DataDirectory[0]
    # (export) at +112, DataDirectory[1] (IMPORT) at +120. Reading +112 yields
    # the export table, which is empty for an exe — that silently reported
    # "imports: (none)" and would have let a bad binary pass as good.
    imports_rva, imports_sz = struct.unpack_from("<II", data, opt_off + 120)
    sections = []
    for i in range(n_sec):
        o = sec_off + i*40
        name = data[o:o+8].rstrip(b"\0").decode("ascii", "replace")
        vsize, va, rawsz, rawptr = struct.unpack_from("<IIII", data, o+8)
        sections.append((name, va, vsize, rawptr, rawsz))

    def rva_to_off(rva):
        for _, va, vsize, rawptr, rawsz in sections:
            if va <= rva < va + max(vsize, rawsz):
                return rawptr + (rva - va)
        return None

    dlls = []
    if imports_sz:
        io = rva_to_off(imports_rva)
        if io is None:
            return fail("import directory RVA does not map to any section")
        while True:
            # IMAGE_IMPORT_DESCRIPTOR layout: OriginalFirstThunk(+0),
            # TimeDateStamp(+4), ForwarderChain(+8), Name(+12), FirstThunk(+16).
            # Reading +0 yields the thunk table RVA, which decodes as garbage
            # DLL names — an earlier revision of this script did exactly that.
            name_rva = struct.unpack_from("<I", data, io + 12)[0]
            if name_rva == 0:
                break
            no = rva_to_off(name_rva)
            if no is None:
                break
            end = data.index(b"\0", no)
            dlls.append(data[no:end].decode("ascii", "replace").lower())
            io += 20

    bad = [d for d in dlls if d not in UNIVERSAL_DLLS]
    sub_name = {1:"NATIVE",2:"GUI",3:"CONSOLE",9:"EFI"}.get(subsystem, str(subsystem))
    print(f"   PE32+ x64, subsystem={sub_name}, {n_sec} sections, {len(data)} bytes")
    print(f"   imports: {', '.join(dlls) if dlls else '(none)'}")
    if bad:
        return fail(f"non-universal imports would break clean installs: {bad}")
    # Only the shipped game binary must touch the GL/windowing layer. A headless
    # console test legitimately links just kernel32 + the C runtime.
    if want_sub == 2 and "opengl32.dll" not in dlls and "user32.dll" not in dlls:
        return fail("game exe expected to import opengl32.dll or user32.dll")
    print(f"   ✓ PE verified: Win64 {sub_name} exe, imports only OS-shipped DLLs")
    return 0

if __name__ == "__main__":
    if len(sys.argv) not in (2, 3):
        print("usage: verify_pe.py <file.exe> [expected_subsystem=2]")
        sys.exit(2)
    sys.exit(main(sys.argv[1]))
