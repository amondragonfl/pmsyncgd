/* live.h - patching a running Geometry Dash in memory (nothing on disk changes). */
#ifndef PMSYNCGD_LIVE_H
#define PMSYNCGD_LIVE_H

#include <stddef.h>
#include <stdint.h>

#include "patcher.h"

typedef struct {
    uint32_t text_rva;   /* to print sites as GeometryDash.exe+offset */
    pm_result scan;      /* what we found, before patching */
} live_result;

unsigned long proc_find_gd(void);   /* pid of a running GeometryDash.exe, or 0 */

/* Patches every unpatched Music Unlocker check in the running game.
   Returns 1 on success; on failure fills `why` with a friendly reason. */
int live_patch(unsigned long pid, live_result *out, char *why, size_t cap);

/* Blocks for up to `ms` milliseconds; returns 1 once the process is gone. */
int proc_wait_exit(unsigned long pid, unsigned ms);

#endif
