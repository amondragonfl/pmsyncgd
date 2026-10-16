/* patcher.c - finds (and flips) the Music Unlocker checks inside GeometryDash.exe. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "patcher.h"

/* mov edx, 0Ch ; mov r8d, <id> */
static const uint8_t k_prefix[] = { 0xBA, 0x0C, 0x00, 0x00, 0x00, 0x41, 0xB8 };

/* mov al, 1 ; nop ; nop ; nop */
const uint8_t PM_PATCH[PM_SLOT] = { 0xB0, 0x01, 0x90, 0x90, 0x90 };

#define ID_OFF     7                          /* low byte of the item id */
#define SLOT_OFF   11                         /* the 5-byte call */
#define MATCH_LEN  (SLOT_OFF + PM_SLOT + 2)   /* + the 2-byte bool check after it */
#define MIN_VOTES  3

static int32_t rd32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

static int is_gjitem_check(const uint8_t *p)
{
    return memcmp(p, k_prefix, sizeof k_prefix) == 0 && p[8] == 0 && p[9] == 0 && p[10] == 0;
}

/* `test al, al` or `xor al, 1`: the caller only looks at the bool in al */
static int uses_bool(const uint8_t *p)
{
    return (p[0] == 0x84 && p[1] == 0xC0) || (p[0] == 0x34 && p[1] == 0x01);
}

static int64_t call_target(const uint8_t *code, size_t slot)
{
    return (int64_t)slot + PM_SLOT + rd32(code + slot + 1);
}

int pm_find_text(const uint8_t *p, size_t n, pm_text *t)
{
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)p;
    const IMAGE_NT_HEADERS64 *nt;
    const IMAGE_SECTION_HEADER *sec;
    size_t lfanew, sec_off;
    unsigned i, nsec;

    if (n < sizeof *dos || dos->e_magic != IMAGE_DOS_SIGNATURE)
        return 0;
    lfanew = (size_t)(uint32_t)dos->e_lfanew;
    if (lfanew > n || n - lfanew < sizeof *nt)
        return 0;
    nt = (const IMAGE_NT_HEADERS64 *)(p + lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64)
        return 0;

    nsec = nt->FileHeader.NumberOfSections;
    sec_off = lfanew + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt->FileHeader.SizeOfOptionalHeader;
    if (sec_off > n || (n - sec_off) / sizeof *sec < nsec)
        return 0;
    sec = (const IMAGE_SECTION_HEADER *)(p + sec_off);

    for (i = 0; i < nsec; i++) {
        if (memcmp(sec[i].Name, ".text", 6) == 0) {
            t->rva = sec[i].VirtualAddress;
            t->vsize = sec[i].Misc.VirtualSize;
            t->raw = sec[i].PointerToRawData;
            t->rawsize = sec[i].SizeOfRawData;
            return 1;
        }
    }
    return 0;
}

void pm_scan(const uint8_t *code, size_t len, pm_result *r)
{
    struct { int64_t target; int votes; } cand[8];
    int ncand = 0, best = -1, k;
    size_t i;

    memset(r, 0, sizeof *r);
    if (len < MATCH_LEN)
        return;

    /* Pass 1: every GJItem check that consumes a bool votes for its call target.
       isItemUnlocked wins by a landslide; this keeps us from hard-coding an address. */
    for (i = 0; i + MATCH_LEN <= len; i++) {
        size_t slot = i + SLOT_OFF;
        int64_t t;

        if (!is_gjitem_check(code + i) || code[slot] != 0xE8 || !uses_bool(code + slot + PM_SLOT))
            continue;
        t = call_target(code, slot);
        for (k = 0; k < ncand && cand[k].target != t; k++)
            ;
        if (k == ncand) {
            if (ncand == (int)(sizeof cand / sizeof cand[0]))
                continue;
            cand[ncand].target = t;
            cand[ncand].votes = 0;
            ncand++;
        }
        cand[k].votes++;
    }
    for (k = 0; k < ncand; k++)
        if (best < 0 || cand[k].votes > cand[best].votes)
            best = k;
    if (best >= 0 && cand[best].votes >= MIN_VOTES) {
        r->have_isu = 1;
        r->isu = cand[best].target;
    }

    /* Pass 2: the Music Unlocker checks, patched or not. A same-looking call to
       GameStatsManager::toggleEnableItem(GJItem, 17, ...) is skipped because it
       neither targets isItemUnlocked nor tests a bool afterwards. */
    for (i = 0; i + MATCH_LEN <= len && r->count < PM_MAX_SITES; i++) {
        size_t slot = i + SLOT_OFF;
        int patched;

        if (!is_gjitem_check(code + i) || code[i + ID_OFF] != PM_MUSIC_UNLOCKER ||
            !uses_bool(code + slot + PM_SLOT))
            continue;
        if (memcmp(code + slot, PM_PATCH, PM_SLOT) == 0)
            patched = 1;
        else if (r->have_isu && code[slot] == 0xE8 && call_target(code, slot) == r->isu)
            patched = 0;
        else
            continue;
        r->sites[r->count].slot = (uint32_t)slot;
        r->sites[r->count].patched = patched;
        r->count++;
        r->patched += patched;
    }
}

void pm_original_bytes(uint8_t out[PM_SLOT], uint32_t slot, int64_t isu)
{
    uint32_t rel = (uint32_t)(int32_t)(isu - ((int64_t)slot + PM_SLOT));

    out[0] = 0xE8;
    out[1] = (uint8_t)rel;
    out[2] = (uint8_t)(rel >> 8);
    out[3] = (uint8_t)(rel >> 16);
    out[4] = (uint8_t)(rel >> 24);
}

int pm_load(const wchar_t *path, pm_image *img)
{
    HANDLE h;
    LARGE_INTEGER sz;
    size_t got = 0;

    memset(img, 0, sizeof *img);
    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return PM_E_OPEN;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > (256LL << 20)) {
        CloseHandle(h);
        return PM_E_FORMAT;
    }
    img->size = (size_t)sz.QuadPart;
    if (!(img->data = malloc(img->size))) {
        CloseHandle(h);
        return PM_E_MEMORY;
    }
    while (got < img->size) {
        DWORD n;
        if (!ReadFile(h, img->data + got, (DWORD)(img->size - got), &n, NULL) || !n)
            break;
        got += n;
    }
    CloseHandle(h);
    if (got != img->size) {
        pm_free(img);
        return PM_E_READ;
    }

    if (!pm_find_text(img->data, img->size, &img->text) || img->text.raw > img->size) {
        pm_free(img);
        return PM_E_FORMAT;
    }
    img->code = img->data + img->text.raw;
    img->code_len = img->text.vsize < img->text.rawsize ? img->text.vsize : img->text.rawsize;
    if (img->code_len > img->size - img->text.raw)
        img->code_len = img->size - img->text.raw;
    return PM_OK;
}

void pm_free(pm_image *img)
{
    free(img->data);
    memset(img, 0, sizeof *img);
}

const char *pm_strerror(int e)
{
    switch (e) {
    case PM_OK:       return "ok";
    case PM_E_OPEN:   return "couldn't open the file";
    case PM_E_READ:   return "couldn't read the whole file";
    case PM_E_FORMAT: return "that doesn't look like a 64-bit GeometryDash.exe";
    case PM_E_MEMORY: return "ran out of memory";
    }
    return "unknown error";
}

int pm_write_at(void *file, uint64_t off, const uint8_t *bytes, uint32_t n)
{
    LARGE_INTEGER pos;
    DWORD wrote;

    pos.QuadPart = (LONGLONG)off;
    return SetFilePointerEx((HANDLE)file, pos, NULL, FILE_BEGIN) &&
           WriteFile((HANDLE)file, bytes, n, &wrote, NULL) && wrote == n;
}
