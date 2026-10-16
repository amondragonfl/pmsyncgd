/* gdfind.c - locating the Geometry Dash install. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "gdfind.h"

#define GD_APPID_MANIFEST L"appmanifest_322170.acf"
#define GD_SUBDIR         L"steamapps\\common\\Geometry Dash"

int gd_join(wchar_t *out, size_t cap, const wchar_t *a, const wchar_t *b)
{
    size_t la = wcslen(a), lb = wcslen(b);
    int sep = la && a[la - 1] != L'\\' && a[la - 1] != L'/';

    if (la + sep + lb + 1 > cap)
        return 0;
    memmove(out, a, la * sizeof *out);
    if (sep)
        out[la++] = L'\\';
    memcpy(out + la, b, (lb + 1) * sizeof *out);
    return 1;
}

int gd_file_exists(const wchar_t *path)
{
    DWORD a = GetFileAttributesW(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static void tidy(wchar_t *p)
{
    for (; *p; p++)
        if (*p == L'/')
            *p = L'\\';
}

/* also drops a stray quote: cmd turns --path "C:\Games\GD\" into C:\Games\GD" */
static void strip_trailing_slashes(wchar_t *p)
{
    size_t n = wcslen(p);
    while (n > 3 && (p[n - 1] == L'\\' || p[n - 1] == L'/' || p[n - 1] == L'"'))
        p[--n] = 0;
}

/* Steam stores paths in lowercase; ask Windows for the real spelling, for display. */
static void canonical(wchar_t *path)
{
    HANDLE h = CreateFileW(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    wchar_t buf[GD_PATH_MAX];
    const wchar_t *p = buf;
    DWORD n;

    if (h == INVALID_HANDLE_VALUE)
        return;
    n = GetFinalPathNameByHandleW(h, buf, GD_PATH_MAX, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    CloseHandle(h);
    if (n == 0 || n >= GD_PATH_MAX || !wcsncmp(buf, L"\\\\?\\UNC\\", 8))
        return;
    if (!wcsncmp(buf, L"\\\\?\\", 4))
        p += 4;
    wcscpy(path, p);
}

static int try_dir(const wchar_t *candidate, wchar_t *dir, size_t cap)
{
    wchar_t buf[GD_PATH_MAX], exe[GD_PATH_MAX];

    if (wcslen(candidate) >= GD_PATH_MAX)
        return 0;
    wcscpy(buf, candidate);
    tidy(buf);
    strip_trailing_slashes(buf);
    if (!gd_join(exe, GD_PATH_MAX, buf, GD_EXE_NAME) || !gd_file_exists(exe))
        return 0;
    canonical(buf);
    if (wcslen(buf) >= cap)
        return 0;
    wcscpy(dir, buf);
    return 1;
}

static char *read_text_file(const wchar_t *path)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    LARGE_INTEGER sz;
    DWORD got = 0;
    char *buf = NULL;

    if (h == INVALID_HANDLE_VALUE)
        return NULL;
    if (GetFileSizeEx(h, &sz) && sz.QuadPart > 0 && sz.QuadPart < (1 << 20) &&
        (buf = malloc((size_t)sz.QuadPart + 1))) {
        if (!ReadFile(h, buf, (DWORD)sz.QuadPart, &got, NULL))
            got = 0;
        buf[got] = 0;
    }
    CloseHandle(h);
    return buf;
}

/* Reads the quoted value following `"key"` in a Valve KeyValues text file,
   un-escaping backslashes. Returns a pointer just past it, or NULL. */
static const char *vdf_next(const char *p, const char *key, char *val, size_t cap)
{
    size_t o = 0;

    if (!(p = strstr(p, key)) || !(p = strchr(p + strlen(key), '"')))
        return NULL;
    for (p++; *p && *p != '"'; p++) {
        if (*p == '\\' && p[1])
            p++;
        if (o + 1 < cap)
            val[o++] = *p;
    }
    val[o] = 0;
    return *p ? p + 1 : p;
}

static int try_steam_root(const wchar_t *root, wchar_t *dir, size_t cap)
{
    wchar_t buf[GD_PATH_MAX];
    char *vdf, lib[GD_PATH_MAX];
    const char *p;
    int found = 0;

    if (gd_join(buf, GD_PATH_MAX, root, GD_SUBDIR) && try_dir(buf, dir, cap))
        return 1;

    /* other library folders (e.g. D:\SteamLibrary) */
    if (!gd_join(buf, GD_PATH_MAX, root, L"steamapps\\libraryfolders.vdf") || !(vdf = read_text_file(buf)))
        return 0;
    for (p = vdf; !found && (p = vdf_next(p, "\"path\"", lib, sizeof lib));) {
        wchar_t wlib[GD_PATH_MAX];
        if (MultiByteToWideChar(CP_UTF8, 0, lib, -1, wlib, GD_PATH_MAX) &&
            gd_join(buf, GD_PATH_MAX, wlib, GD_SUBDIR))
            found = try_dir(buf, dir, cap);
    }
    free(vdf);
    return found;
}

static int reg_path(HKEY root, const wchar_t *key, const wchar_t *name, wchar_t *out)
{
    DWORD bytes = GD_PATH_MAX * sizeof *out;
    return RegGetValueW(root, key, name, RRF_RT_REG_SZ, NULL, out, &bytes) == ERROR_SUCCESS;
}

int gd_find(const wchar_t *hint, wchar_t *dir, size_t cap)
{
    wchar_t buf[GD_PATH_MAX];
    wchar_t *slash;
    DWORD n;

    if (hint) {
        if (wcslen(hint) >= GD_PATH_MAX)
            return 0;
        wcscpy(buf, hint);
        tidy(buf);
        /* accept the path to the exe itself, too */
        if ((slash = wcsrchr(buf, L'\\')) && _wcsicmp(slash + 1, GD_EXE_NAME) == 0)
            *slash = 0;
        return try_dir(buf, dir, cap);
    }

    /* dropped right into the game folder? */
    n = GetModuleFileNameW(NULL, buf, GD_PATH_MAX);
    if (n && n < GD_PATH_MAX && (slash = wcsrchr(buf, L'\\'))) {
        *slash = 0;
        if (try_dir(buf, dir, cap))
            return 1;
    }
    n = GetCurrentDirectoryW(GD_PATH_MAX, buf);
    if (n && n < GD_PATH_MAX && try_dir(buf, dir, cap))
        return 1;

    if (reg_path(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", buf) && try_steam_root(buf, dir, cap))
        return 1;
    if (reg_path(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath", buf) &&
        try_steam_root(buf, dir, cap))
        return 1;
    if (reg_path(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", L"InstallPath", buf) && try_steam_root(buf, dir, cap))
        return 1;

    return try_steam_root(L"C:\\Program Files (x86)\\Steam", dir, cap);
}

int gd_steam_build(const wchar_t *dir, char *out, size_t cap)
{
    wchar_t buf[GD_PATH_MAX];
    wchar_t *slash;
    char *acf;
    int ok;

    /* <library>\steamapps\common\Geometry Dash -> <library>\steamapps */
    if (wcslen(dir) >= GD_PATH_MAX)
        return 0;
    wcscpy(buf, dir);
    if (!(slash = wcsrchr(buf, L'\\')))
        return 0;
    *slash = 0;
    if (!(slash = wcsrchr(buf, L'\\')))
        return 0;
    *slash = 0;
    if (!gd_join(buf, GD_PATH_MAX, buf, GD_APPID_MANIFEST) || !(acf = read_text_file(buf)))
        return 0;
    ok = vdf_next(acf, "\"buildid\"", out, cap) != NULL && out[0];
    free(acf);
    return ok;
}
