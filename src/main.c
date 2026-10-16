/* main.c - pmsyncgd: unlocks Geometry Dash's "Practice Music Sync" toggle
 * without needing the Music Unlocker from the shop.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "gdfind.h"
#include "live.h"
#include "patcher.h"
#include "ui.h"

#define PMSYNCGD_VERSION "1.0.0"
#define BACKUP_SUFFIX    L".pmsyncgd-backup"

typedef struct {
    wchar_t dir[GD_PATH_MAX];
    wchar_t exe[GD_PATH_MAX];
    wchar_t bak[GD_PATH_MAX];
} gd_paths;

typedef enum { ST_NO_GAME, ST_UNREADABLE, ST_UNKNOWN, ST_OFF, ST_ON, ST_PARTIAL } mod_state;

static const wchar_t *opt_path;
static int opt_once;

/* What each check does, for the build pmsyncgd was made on (Steam build 25909607).
   Other builds still work; their locks just don't get a name. */
static const struct { uint32_t rva; const char *what; } k_known[] = {
    { 0x2c207b, "the toggle in the options menu" },
    { 0x2c4695, "reading the toggle" },
    { 0x2c49db, "flipping the toggle" },
    { 0x2c4a68, "the \"you need a Music Unlocker\" popup" },
    { 0x50da85, "music sync inside levels" },
};

static const char *site_name(uint32_t rva)
{
    size_t i;
    for (i = 0; i < sizeof k_known / sizeof k_known[0]; i++)
        if (k_known[i].rva == rva)
            return k_known[i].what;
    return "a Music Unlocker check";
}

static void site_line(int i, int n, uint32_t rva, const char *result)
{
    say("      {p}\u266A{/} lock %d/%d {g}%s{/}  {c}%s{/} {d}@ GeometryDash.exe+0x%06lx{/}\n",
        i, n, result, site_name(rva), (unsigned long)rva);
    nap(160);
}

static void explain_not_found(void)
{
    oops("couldn't find Geometry Dash {d}(o_o){/}");
    if (opt_path)
        info("there's no GeometryDash.exe in {c}%s{/}", u8(opt_path));
    info("tip: tell me where it is with {c}--path \"D:\\Games\\Geometry Dash\"{/}");
    info("     or drop pmsyncgd.exe into the game folder and run it from there");
}

static int locate(gd_paths *gp, int quiet)
{
    if (!gd_find(opt_path, gp->dir, GD_PATH_MAX) || !gd_join(gp->exe, GD_PATH_MAX, gp->dir, GD_EXE_NAME) ||
        wcslen(gp->exe) + wcslen(BACKUP_SUFFIX) >= GD_PATH_MAX) {
        if (!quiet)
            explain_not_found();
        return 0;
    }
    wcscpy(gp->bak, gp->exe);
    wcscat(gp->bak, BACKUP_SUFFIX);
    return 1;
}

static int load_and_scan(const gd_paths *gp, pm_image *img, pm_result *r)
{
    int e = pm_load(gp->exe, img);

    if (e != PM_OK) {
        if (e == PM_E_OPEN)
            oops("couldn't open GeometryDash.exe {d}(error %lu){/}", GetLastError());
        else
            oops("couldn't read GeometryDash.exe: %s", pm_strerror(e));
        return 0;
    }
    pm_scan(img->code, img->code_len, r);
    return 1;
}

static mod_state probe(gd_paths *gp, pm_result *r)
{
    pm_image img;

    memset(r, 0, sizeof *r);
    if (!locate(gp, 1))
        return ST_NO_GAME;
    if (pm_load(gp->exe, &img) != PM_OK)
        return ST_UNREADABLE;
    pm_scan(img.code, img.code_len, r);
    pm_free(&img);
    if (r->count == 0)
        return ST_UNKNOWN;
    if (r->patched == 0)
        return ST_OFF;
    return r->patched == r->count ? ST_ON : ST_PARTIAL;
}

static const char *state_text(mod_state s)
{
    switch (s) {
    case ST_NO_GAME:    return "{y}can't find Geometry Dash{/}";
    case ST_UNREADABLE: return "{y}can't read GeometryDash.exe{/}";
    case ST_UNKNOWN:    return "{y}unknown GD version (no Music Unlocker checks found){/}";
    case ST_OFF:        return "{d}not installed{/}";
    case ST_ON:         return "{g}installed \u2665{/}";
    case ST_PARTIAL:    return "{y}half installed - run install again{/}";
    }
    return "";
}

static void unsupported(void)
{
    oops("I couldn't find the Music Unlocker checks in this GeometryDash.exe {d}(o_o){/}");
    info("this GD build is probably newer (or older) than the one pmsyncgd knows.");
    info("nothing was changed. an issue on GitHub with your GD version would help a lot!");
}

static void cant_write(unsigned long err)
{
    if (err == ERROR_ACCESS_DENIED) {
        oops("Windows won't let me write to the game folder {d}(>_<){/}");
        info("right-click pmsyncgd.exe {c}\u203A Run as administrator{/} and try again");
    } else if (err == ERROR_SHARING_VIOLATION) {
        oops("GeometryDash.exe is busy - is the game (or a Steam update) still running?");
    } else {
        oops("couldn't open GeometryDash.exe for writing {d}(error %lu){/}", err);
    }
}

static void wait_until_closed(void)
{
    if (!proc_find_gd()) {
        good("Geometry Dash is closed, all quiet");
        return;
    }
    warn("Geometry Dash is running! close it and I'll carry on {d}(^_^){/}");
    while (proc_find_gd()) {
        spin("waiting for GD to close...");
        Sleep(200);
    }
    spin_done();
    good("thanks! GD is closed");
    Sleep(800);   /* give Windows a moment to let go of the file */
}

static HANDLE open_exclusive(const wchar_t *path)
{
    int tries;

    for (tries = 0; tries < 20; tries++) {
        HANDLE h = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE || GetLastError() != ERROR_SHARING_VIOLATION)
            return h;
        Sleep(250);
    }
    return INVALID_HANDLE_VALUE;
}

static void how_to_use(int live)
{
    say("\n  {b}how to switch it on:{/}\n"
        "    {p}1.{/} %s\n"
        "    {p}2.{/} jump into any level and pause it {d}(Esc){/}\n"
        "    {p}3.{/} open the {b}options{/} {d}(the gear in the pause menu){/} and flip through the pages\n"
        "    {p}4.{/} turn on {c}Practice Music Sync{/} and hop into practice mode\n"
        "    {p}5.{/} enjoy the real song, perfectly in sync {p}\u266A\u266B{/}\n",
        live ? "keep Geometry Dash open (it's already patched!)" : "open Geometry Dash");
    if (live)
        say("\n  {d}live mode lasts until GD closes - run me again next time,\n"
            "  or pick \"install\" to make it stick.{/}\n");
    else
        say("\n  {d}heads-up: when Steam updates GD, the mod gets washed away - just run me again!\n"
            "  no need to buy the Music Unlocker; the shop may still offer it, that's normal.{/}\n");
}

static int do_install(void)
{
    gd_paths gp;
    pm_image img;
    pm_result r;
    HANDLE h;
    int i;

    step(1, 5, "looking for Geometry Dash...");
    if (!locate(&gp, 0))
        return 1;
    good("found it! {d}%s{/}", u8(gp.dir));

    step(2, 5, "making sure the game is closed...");
    wait_until_closed();

    step(3, 5, "sniffing out the Music Unlocker checks...");
    if (!load_and_scan(&gp, &img, &r))
        return 1;
    info("{d}read GeometryDash.exe (%.1f MB) and searched its code{/}", img.size / 1048576.0);
    if (r.count == 0) {
        pm_free(&img);
        unsupported();
        return 1;
    }
    if (r.patched == r.count) {
        pm_free(&img);
        good("the mod is already installed - all %d locks are open {p}\u2665{/}", r.count);
        how_to_use(0);
        return 0;
    }
    good("found %d little locks on the music box {p}\u266A{/}", r.count);

    step(4, 5, "tucking a backup under the pillow...");
    if (r.patched == 0) {
        /* a clean original: (re)take the backup, so it always matches the current GD version */
        if (!CopyFileW(gp.exe, gp.bak, FALSE)) {
            oops("couldn't save a backup {d}(error %lu){/}, so I didn't touch anything", GetLastError());
            pm_free(&img);
            return 1;
        }
        good("saved {c}GeometryDash.exe%s{/} next to the game", u8(BACKUP_SUFFIX));
    } else if (gd_file_exists(gp.bak)) {
        good("the backup from last time is still there");
    } else {
        warn("no backup around - Steam's \"Verify integrity of game files\" can always restore GD");
    }

    step(5, 5, "picking the locks...");
    h = open_exclusive(gp.exe);
    if (h == INVALID_HANDLE_VALUE) {
        cant_write(GetLastError());
        pm_free(&img);
        return 1;
    }
    for (i = 0; i < r.count; i++) {
        const pm_site *s = &r.sites[i];
        if (!s->patched && !pm_write_at(h, (uint64_t)img.text.raw + s->slot, PM_PATCH, PM_SLOT)) {
            oops("couldn't write lock %d {d}(error %lu){/}", i + 1, GetLastError());
            info("run me again, or pick uninstall to undo what got done");
            CloseHandle(h);
            pm_free(&img);
            return 1;
        }
        site_line(i + 1, r.count, img.text.rva + s->slot, s->patched ? "was open" : "open!");
    }
    FlushFileBuffers(h);
    CloseHandle(h);
    pm_free(&img);

    if (!load_and_scan(&gp, &img, &r))
        return 1;
    pm_free(&img);
    if (r.count == 0 || r.patched != r.count) {
        oops("double-check failed: only %d/%d locks are open {d}(>_<){/}", r.patched, r.count);
        return 1;
    }
    good("double-checked from disk: %d/%d locks open", r.patched, r.count);

    say("\n  {p}\\(^o^)/{/}  {b}practice music sync is unlocked!{/}\n");
    how_to_use(0);
    return 0;
}

static int do_uninstall(void)
{
    gd_paths gp;
    pm_image img, bak;
    pm_result r;
    uint8_t orig[PM_SLOT];
    HANDLE h;
    int i, n;

    step(1, 4, "looking for Geometry Dash...");
    if (!locate(&gp, 0))
        return 1;
    good("found it! {d}%s{/}", u8(gp.dir));

    step(2, 4, "making sure the game is closed...");
    wait_until_closed();

    step(3, 4, "looking for the locks I opened...");
    if (!load_and_scan(&gp, &img, &r))
        return 1;
    if (r.patched == 0) {
        pm_free(&img);
        good("nothing to undo - the mod isn't installed {d}(^_^){/}");
        return 0;
    }
    if (!r.have_isu) {
        pm_free(&img);
        oops("couldn't work out the original code {d}(>_<){/}");
        info("Steam can fix it: right-click Geometry Dash {c}\u203A Properties \u203A Installed Files \u203A Verify integrity{/}");
        return 1;
    }
    good("found %d open locks", r.patched);

    step(4, 4, "putting the locks back...");
    h = open_exclusive(gp.exe);
    if (h == INVALID_HANDLE_VALUE) {
        cant_write(GetLastError());
        pm_free(&img);
        return 1;
    }
    for (i = 0, n = 0; i < r.count; i++) {
        const pm_site *s = &r.sites[i];
        if (!s->patched)
            continue;
        pm_original_bytes(orig, s->slot, r.isu);
        if (!pm_write_at(h, (uint64_t)img.text.raw + s->slot, orig, PM_SLOT)) {
            oops("couldn't restore lock %d {d}(error %lu){/}", i + 1, GetLastError());
            CloseHandle(h);
            pm_free(&img);
            return 1;
        }
        site_line(++n, r.patched, img.text.rva + s->slot, "closed");
    }
    FlushFileBuffers(h);
    CloseHandle(h);
    pm_free(&img);

    if (!load_and_scan(&gp, &img, &r))
        return 1;
    if (r.patched != 0) {
        pm_free(&img);
        oops("double-check failed: %d locks are still open {d}(>_<){/}", r.patched);
        return 1;
    }
    good("double-checked from disk: every lock is closed");

    /* if we're now byte-for-byte the original, the backup has done its job */
    if (gd_file_exists(gp.bak) && pm_load(gp.bak, &bak) == PM_OK) {
        int same = bak.size == img.size && memcmp(bak.data, img.data, img.size) == 0;
        pm_free(&bak);
        if (same && DeleteFileW(gp.bak))
            good("the game file matches the backup exactly, so I tidied the backup away");
        else if (!same)
            info("{d}kept the old backup (it's from a different GD version) - delete it whenever you like{/}");
    }
    pm_free(&img);

    say("\n  {p}(^_^)/~{/}  {b}all back to vanilla.{/} bye bye, music sync!\n");
    return 0;
}

static int do_live(void)
{
    gd_paths gp;
    pm_result r;
    live_result lr;
    char why[256];
    unsigned long pid;
    int round, i;

    step(1, 3, "peeking at the game file first...");
    switch (probe(&gp, &r)) {
    case ST_ON:
        good("the mod is already installed on disk {p}\u2665{/} no need for live mode!");
        how_to_use(0);
        return 0;
    case ST_OFF:
    case ST_PARTIAL:
        good("live mode won't change any files - the patch only lives in the game's memory");
        break;
    default:
        info("{d}couldn't check the game file - that's ok, live mode doesn't need it{/}");
    }

    for (round = 0;; round++) {
        if (round == 0) {
            step(2, 3, "waiting for Geometry Dash to start...");
            info("{d}launch it from Steam whenever you're ready (press any key to give up){/}");
        } else {
            say("\n  {p}\u266A{/} {b}waiting for Geometry Dash to start again...{/}\n");
        }
        while (!(pid = proc_find_gd())) {
            if (key_pressed()) {
                spin_done();
                info("okay, stopping here {d}(^_^)/{/}");
                return 0;
            }
            spin("(-_-) zzz  waiting for GD...");
            Sleep(200);
        }
        spin_done();
        good("there it is! {d}(pid %lu){/}", pid);

        if (round == 0)
            step(3, 3, "sneaking into the game's memory...");
        if (!live_patch(pid, &lr, why, sizeof why)) {
            oops("%s {d}(>_<){/}", why);
            return 1;
        }
        for (i = 0; i < lr.scan.count; i++)
            site_line(i + 1, lr.scan.count, lr.text_rva + lr.scan.sites[i].slot,
                      lr.scan.sites[i].patched ? "was open" : "open!");
        say("\n  {p}\\(^o^)/{/}  {b}practice music sync is unlocked for this session!{/}\n");
        if (round == 0)
            how_to_use(1);
        if (opt_once)
            return 0;

        say("\n  {d}I'll keep watching in case GD restarts. press any key (or close this window) to stop.{/}\n");
        while (!proc_wait_exit(pid, 200)) {
            if (key_pressed()) {
                spin_done();
                info("okay, stopping - GD stays unlocked until you close it {d}(^_^)/{/}");
                return 0;
            }
            spin("jamming along with GD...");
        }
        spin_done();
        info("{d}GD closed, and the live patch went with it.{/}");
    }
}

static int do_status(void)
{
    gd_paths gp;
    pm_result r;
    char build[64];
    unsigned long pid;
    mod_state s = probe(&gp, &r);

    say("\n  {p}\u266A{/} {b}status check{/}\n");
    if (s == ST_NO_GAME) {
        explain_not_found();
        return 1;
    }
    row("game folder", "%s", u8(gp.dir));
    if (gd_steam_build(gp.dir, build, sizeof build))
        row("steam build", "%s", build);
    if (s == ST_ON || s == ST_OFF || s == ST_PARTIAL)
        row("mod", "%s {d}(%d/%d locks open){/}", state_text(s), r.patched, r.count);
    else
        row("mod", "%s", state_text(s));
    if (gd_file_exists(gp.bak))
        row("backup", "yes {d}(GeometryDash.exe%s){/}", u8(BACKUP_SUFFIX));
    else
        row("backup", "none");
    if ((pid = proc_find_gd()))
        row("game", "running {d}(pid %lu){/}", pid);
    else
        row("game", "not running");
    return 0;
}

static void usage(void)
{
    say("\n  {b}usage:{/} pmsyncgd {c}[install | uninstall | live | status]{/} {d}[options]{/}\n\n"
        "    {c}install{/}       unlock the toggle by patching GeometryDash.exe (keeps a backup)\n"
        "    {c}uninstall{/}     put GeometryDash.exe back the way it was\n"
        "    {c}live{/}          unlock it in the running game only - no files touched\n"
        "    {c}status{/}        show what's going on\n"
        "    {d}no command opens a friendly menu{/}\n\n"
        "  {b}options:{/}\n"
        "    {c}--path <dir>{/}  where Geometry Dash lives, if I can't find it myself\n"
        "    {c}--once{/}        live mode: patch once and exit instead of watching\n"
        "    {c}--fast{/}        skip the dramatic pauses\n\n"
        "  {d}pmsyncgd %s \u00B7 https://github.com/amondragonfl/pmsyncgd{/}\n\n", PMSYNCGD_VERSION);
}

static int menu(void)
{
    char pick[32];
    gd_paths gp;
    pm_result r;
    int rc = 0;

    for (;;) {
        say("\n  {d}mod status:{/} %s\n", state_text(probe(&gp, &r)));
        say("\n  {b}what shall we do?{/}\n\n"
            "    {p}[1]{/} install     {d}unlock the toggle for good (patches GeometryDash.exe, keeps a backup){/}\n"
            "    {p}[2]{/} uninstall   {d}put everything back the way it was{/}\n"
            "    {p}[3]{/} live mode   {d}unlock it in the running game only, no files touched{/}\n"
            "    {p}[4]{/} status      {d}peek at what's going on{/}\n"
            "    {p}[q]{/} quit\n");
        if (!ask("\n  {p}\u266A{/} pick one {p}\u203A{/} ", pick, sizeof pick))
            return rc;
        switch (pick[0]) {
        case '1': rc = do_install(); break;
        case '2': rc = do_uninstall(); break;
        case '3': rc = do_live(); break;
        case '4': rc = do_status(); break;
        case 'q':
        case 'Q':
            say("\n  bye bye! {p}(^_^)/{/}\n\n");
            return rc;
        default:
            say("\n  {y}(o_o)?{/} that's not on the menu - try 1, 2, 3, 4 or q\n");
            continue;
        }
        wait_enter("\n  {d}press Enter to go back to the menu...{/}");
    }
}

int wmain(int argc, wchar_t **argv)
{
    const wchar_t *cmd = NULL;
    int fast = 0, i, rc;

    for (i = 1; i < argc; i++) {
        const wchar_t *a = argv[i];

        if (!wcscmp(a, L"--path") || !wcscmp(a, L"-p")) {
            if (++i >= argc) {
                ui_init(1);
                oops("--path needs a folder after it");
                return 2;
            }
            opt_path = argv[i];
        } else if (!wcscmp(a, L"--fast")) {
            fast = 1;
        } else if (!wcscmp(a, L"--once")) {
            opt_once = 1;
        } else if (!wcscmp(a, L"--help") || !wcscmp(a, L"-h") || !wcscmp(a, L"/?")) {
            ui_init(1);
            banner();
            usage();
            return 0;
        } else if (!cmd && a[0] != L'-') {
            cmd = a;
        } else {
            ui_init(1);
            oops("I don't know \"%s\" {d}(o_o){/}", u8(a));
            usage();
            return 2;
        }
    }

    ui_init(fast);
    banner();
    if (!cmd)
        return menu();

    if (!_wcsicmp(cmd, L"install"))
        rc = do_install();
    else if (!_wcsicmp(cmd, L"uninstall"))
        rc = do_uninstall();
    else if (!_wcsicmp(cmd, L"live"))
        rc = do_live();
    else if (!_wcsicmp(cmd, L"status"))
        rc = do_status();
    else {
        oops("I don't know the command \"%s\" {d}(o_o){/}", u8(cmd));
        usage();
        return 2;
    }
    if (ui_owns_console())
        wait_enter("\n  {d}press Enter to close...{/}");
    else
        say("\n");
    return rc;
}
