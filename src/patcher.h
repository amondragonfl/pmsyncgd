/* patcher.h - finds (and flips) the Music Unlocker checks inside GeometryDash.exe.
 *
 * GD guards "Practice Music Sync" with calls shaped like this:
 *
 *     BA 0C 00 00 00        mov  edx, 0Ch          ; UnlockType::GJItem
 *     41 B8 11 00 00 00     mov  r8d, 11h          ; item 17 = Music Unlocker
 *     E8 xx xx xx xx        call GameStatsManager::isItemUnlocked
 *     84 C0 / 34 01         test al, al / xor al, 1
 *
 * The mod swaps the 5-byte call for `mov al, 1 ; nop ; nop ; nop`, so those
 * checks (and only those) always see the Music Unlocker as owned.
 * See docs/how-it-works.md for the full story.
 */
#ifndef PMSYNCGD_PATCHER_H
#define PMSYNCGD_PATCHER_H

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#define PM_SLOT           5    /* bytes in the `call rel32` we swap out */
#define PM_MAX_SITES      32
#define PM_MUSIC_UNLOCKER 17   /* GJItem id of the Music Unlocker shop item */

extern const uint8_t PM_PATCH[PM_SLOT];

typedef struct {
    uint32_t slot;   /* offset of the 5-byte call inside .text */
    int patched;
} pm_site;

typedef struct {
    int have_isu;    /* did we pin down GameStatsManager::isItemUnlocked? */
    int64_t isu;     /* ...its offset relative to .text */
    int count;       /* Music Unlocker checks found */
    int patched;     /* ...of which already patched */
    pm_site sites[PM_MAX_SITES];
} pm_result;

typedef struct {
    uint32_t rva, vsize;     /* where .text lives in memory */
    uint32_t raw, rawsize;   /* where .text lives in the file */
} pm_text;

typedef struct {
    uint8_t *data;
    size_t size;
    pm_text text;
    const uint8_t *code;     /* .text bytes inside data */
    size_t code_len;
} pm_image;

enum { PM_OK, PM_E_OPEN, PM_E_READ, PM_E_FORMAT, PM_E_MEMORY };

int  pm_find_text(const uint8_t *p, size_t n, pm_text *t);
void pm_scan(const uint8_t *code, size_t len, pm_result *r);
void pm_original_bytes(uint8_t out[PM_SLOT], uint32_t slot, int64_t isu);

int  pm_load(const wchar_t *path, pm_image *img);
void pm_free(pm_image *img);
const char *pm_strerror(int e);

/* Writes n bytes at a file offset of an already-open HANDLE. Returns 1 on success. */
int  pm_write_at(void *file, uint64_t off, const uint8_t *bytes, uint32_t n);

#endif
