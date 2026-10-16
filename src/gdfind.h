/* gdfind.h - locating the Geometry Dash install. */
#ifndef PMSYNCGD_GDFIND_H
#define PMSYNCGD_GDFIND_H

#include <stddef.h>
#include <wchar.h>

#define GD_PATH_MAX 1024
#define GD_EXE_NAME L"GeometryDash.exe"

int gd_join(wchar_t *out, size_t cap, const wchar_t *a, const wchar_t *b);
int gd_file_exists(const wchar_t *path);

/* Finds the folder holding GeometryDash.exe. With a hint (--path) only the hint is tried;
   otherwise: our own folder, the current folder, every Steam library, the default path. */
int gd_find(const wchar_t *hint, wchar_t *dir, size_t cap);

/* Best effort: Steam's build id for GD, read from appmanifest_322170.acf. */
int gd_steam_build(const wchar_t *dir, char *out, size_t cap);

#endif
