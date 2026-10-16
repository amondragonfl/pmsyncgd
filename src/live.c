/* live.c - patching a running Geometry Dash in memory (nothing on disk changes). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gdfind.h"
#include "live.h"

unsigned long proc_find_gd(void)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe;
    unsigned long pid = 0;

    if (snap == INVALID_HANDLE_VALUE)
        return 0;
    pe.dwSize = sizeof pe;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, GD_EXE_NAME) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

int proc_wait_exit(unsigned long pid, unsigned ms)
{
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
    DWORD r;

    if (!h)
        return 1;
    r = WaitForSingleObject(h, ms);
    CloseHandle(h);
    return r != WAIT_TIMEOUT;
}

/* A freshly started process may not have its module list ready yet, so retry a bit. */
static uint8_t *module_base(unsigned long pid)
{
    int tries;

    for (tries = 0; tries < 50; tries++) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
        if (snap != INVALID_HANDLE_VALUE) {
            MODULEENTRY32W me;
            uint8_t *base = NULL;

            me.dwSize = sizeof me;
            if (Module32FirstW(snap, &me)) {
                do {
                    if (_wcsicmp(me.szModule, GD_EXE_NAME) == 0) {
                        base = me.modBaseAddr;
                        break;
                    }
                } while (Module32NextW(snap, &me));
            }
            CloseHandle(snap);
            if (base)
                return base;
        }
        Sleep(100);
    }
    return NULL;
}

static int write_patch(HANDLE h, uint8_t *addr)
{
    DWORD old, tmp;
    SIZE_T wrote = 0;
    uint8_t check[PM_SLOT];
    int ok;

    if (!VirtualProtectEx(h, addr, PM_SLOT, PAGE_EXECUTE_READWRITE, &old))
        return 0;
    ok = WriteProcessMemory(h, addr, PM_PATCH, PM_SLOT, &wrote) && wrote == PM_SLOT;
    VirtualProtectEx(h, addr, PM_SLOT, old, &tmp);
    FlushInstructionCache(h, addr, PM_SLOT);
    return ok && ReadProcessMemory(h, addr, check, PM_SLOT, NULL) && memcmp(check, PM_PATCH, PM_SLOT) == 0;
}

int live_patch(unsigned long pid, live_result *out, char *why, size_t cap)
{
    HANDLE h;
    uint8_t *base, *code = NULL;
    uint8_t hdr[0x1000];
    pm_text text;
    SIZE_T got = 0;
    int i, ok = 0;

    memset(out, 0, sizeof *out);
    h = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
                    FALSE, pid);
    if (!h) {
        snprintf(why, cap, "couldn't open the game (error %lu). if GD runs as admin, run me as admin too",
                 GetLastError());
        return 0;
    }

    if (!(base = module_base(pid))) {
        snprintf(why, cap, "couldn't find GeometryDash.exe inside the process");
        goto done;
    }
    if (!ReadProcessMemory(h, base, hdr, sizeof hdr, NULL) || !pm_find_text(hdr, sizeof hdr, &text)) {
        snprintf(why, cap, "couldn't read the game's headers");
        goto done;
    }
    out->text_rva = text.rva;
    if (!(code = malloc(text.vsize)) ||
        !ReadProcessMemory(h, base + text.rva, code, text.vsize, &got) || got != text.vsize) {
        snprintf(why, cap, "couldn't read the game's code");
        goto done;
    }

    pm_scan(code, text.vsize, &out->scan);
    if (out->scan.count == 0) {
        snprintf(why, cap, "no Music Unlocker checks found - this GD version may be unsupported");
        goto done;
    }
    for (i = 0; i < out->scan.count; i++) {
        if (out->scan.sites[i].patched)
            continue;
        if (!write_patch(h, base + text.rva + out->scan.sites[i].slot)) {
            snprintf(why, cap, "couldn't write to the game's memory (error %lu)", GetLastError());
            goto done;
        }
    }
    ok = 1;

done:
    free(code);
    CloseHandle(h);
    return ok;
}
