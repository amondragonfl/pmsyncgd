/* ui.c - small, cute console output helpers. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <conio.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

static HANDLE out;
static int is_console, use_color, fast;

static const struct { char tag; const char *ansi; } tags[] = {
    { 'p', "\x1b[38;5;212m" },
    { 'c', "\x1b[38;5;117m" },
    { 'g', "\x1b[38;5;120m" },
    { 'y', "\x1b[38;5;228m" },
    { 'r', "\x1b[38;5;210m" },
    { 'd', "\x1b[38;5;245m" },
    { 'b', "\x1b[1m" },
    { '/', "\x1b[0m" },
};

void ui_init(int want_fast)
{
    DWORD mode;

    out = GetStdHandle(STD_OUTPUT_HANDLE);
    is_console = GetConsoleMode(out, &mode) != 0;
    if (is_console) {
        SetConsoleOutputCP(CP_UTF8);
        use_color = SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
        SetConsoleTitleW(L"pmsyncgd ♪ practice music sync");
    }
    fast = want_fast || !is_console;
}

/* Console output goes through WriteConsoleW so the music notes and box
   drawing render no matter what code page the console started in. */
static void write_raw(const char *s, size_t n)
{
    if (!n)
        return;
    if (is_console) {
        int wn = MultiByteToWideChar(CP_UTF8, 0, s, (int)n, NULL, 0);
        wchar_t *w = malloc((size_t)wn * sizeof *w);
        DWORD done;

        if (!w)
            return;
        MultiByteToWideChar(CP_UTF8, 0, s, (int)n, w, wn);
        WriteConsoleW(out, w, (DWORD)wn, &done, NULL);
        free(w);
    } else {
        fwrite(s, 1, n, stdout);
        fflush(stdout);
    }
}

static const char *lookup(const char *s)
{
    size_t i;

    if (s[0] != '{' || !s[1] || s[2] != '}')
        return NULL;
    for (i = 0; i < sizeof tags / sizeof tags[0]; i++)
        if (tags[i].tag == s[1])
            return tags[i].ansi;
    return NULL;
}

static void render(const char *s)
{
    /* every tag is 3 chars and expands to at most 11 bytes */
    char *buf = malloc(strlen(s) * 4 + 1);
    size_t o = 0;

    if (!buf)
        return;
    while (*s) {
        const char *ansi = lookup(s);
        if (ansi) {
            if (use_color) {
                size_t n = strlen(ansi);
                memcpy(buf + o, ansi, n);
                o += n;
            }
            s += 3;
            continue;
        }
        buf[o++] = *s++;
    }
    write_raw(buf, o);
    free(buf);
}

static void vsay(const char *fmt, va_list ap)
{
    va_list ap2;
    char *s;
    int n;

    va_copy(ap2, ap);
    n = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (n < 0 || !(s = malloc((size_t)n + 1)))
        return;
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    render(s);
    free(s);
}

void say(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsay(fmt, ap);
    va_end(ap);
}

void step(int n, int total, const char *fmt, ...)
{
    va_list ap;

    say("\n  {p}♪{/} {d}step %d/%d ·{/} {b}", n, total);
    va_start(ap, fmt);
    vsay(fmt, ap);
    va_end(ap);
    say("{/}\n");
    nap(250);
}

#define LINE_WITH_PREFIX(prefix)  \
    va_list ap;                   \
    say("%s", prefix);            \
    va_start(ap, fmt);            \
    vsay(fmt, ap);                \
    va_end(ap);                   \
    say("\n")

void info(const char *fmt, ...) { LINE_WITH_PREFIX("      "); }
void good(const char *fmt, ...) { LINE_WITH_PREFIX("      {g}√{/} "); }
void warn(const char *fmt, ...) { LINE_WITH_PREFIX("      {y}!{/} "); }
void oops(const char *fmt, ...) { LINE_WITH_PREFIX("      {r}x{/} "); }

void row(const char *label, const char *fmt, ...)
{
    va_list ap;

    say("      {d}%-12s{/} ", label);
    va_start(ap, fmt);
    vsay(fmt, ap);
    va_end(ap);
    say("\n");
}

static void clear_line(void)
{
    if (use_color)
        write_raw("\r\x1b[2K", 5);
    else
        say("\r%79s\r", "");
}

void spin(const char *label)
{
    static const char *frames[] = {
        "♪    ", " ♫   ", "  ♪  ", "   ♫ ",
        "    ♪", "   ♫ ", "  ♪  ", " ♫   ",
    };
    static unsigned f;

    if (!is_console)
        return;
    clear_line();
    say("      {p}%s{/} %s", frames[f++ % 8], label);
}

void spin_done(void)
{
    if (is_console)
        clear_line();
}

void nap(int ms)
{
    if (!fast)
        Sleep((DWORD)ms);
}

void banner(void)
{
    say("\n"
        "     {p}┌───────┐{/}\n"
        "     {p}│{/} {c}■   ■{/} {p}│{/}   {b}{p}♪ pmsyncgd ♫{/}\n"
        "     {p}│{/} {c}\\___/{/} {p}│{/}   {d}practice music sync, unlocked{/}\n"
        "     {p}└───────┘{/}   {d}for Geometry Dash on Steam{/}\n");
}

int ask(const char *prompt, char *buf, int cap)
{
    size_t n, skip = 0;

    say("%s", prompt);
    if (!fgets(buf, cap, stdin)) {
        buf[0] = 0;
        return 0;
    }
    /* piped input can start with a UTF-8 BOM */
    if (!strncmp(buf, "\xEF\xBB\xBF", 3))
        skip = 3;
    while (buf[skip] == ' ' || buf[skip] == '\t')
        skip++;
    memmove(buf, buf + skip, strlen(buf + skip) + 1);
    n = strlen(buf);
    while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == ' '))
        buf[--n] = 0;
    return 1;
}

void wait_enter(const char *prompt)
{
    char tmp[64];
    ask(prompt, tmp, sizeof tmp);
}

int key_pressed(void)
{
    if (!is_console || !_kbhit())
        return 0;
    while (_kbhit())
        _getch();
    return 1;
}

int ui_owns_console(void)
{
    DWORD pids[4];
    return is_console && GetConsoleProcessList(pids, 4) <= 1;
}

const char *u8(const wchar_t *w)
{
    static char ring[4][4096];
    static unsigned k;
    char *o = ring[k++ % 4];

    if (!WideCharToMultiByte(CP_UTF8, 0, w, -1, o, (int)sizeof ring[0], NULL, NULL))
        o[0] = 0;
    return o;
}
