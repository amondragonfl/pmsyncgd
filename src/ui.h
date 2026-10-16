/* ui.h - small, cute console output helpers.
 *
 * Every printing function understands a tiny colour markup:
 *   {p} pink   {c} cyan   {g} green   {y} yellow   {r} red   {d} dim   {b} bold   {/} reset
 * Colours are dropped automatically when the console can't show them
 * (or when output is redirected to a file).
 */
#ifndef PMSYNCGD_UI_H
#define PMSYNCGD_UI_H

#include <wchar.h>

#define PRINTF_LIKE(f, a) __attribute__((format(gnu_printf, f, a)))

void ui_init(int fast);

void say(const char *fmt, ...) PRINTF_LIKE(1, 2);
void step(int n, int total, const char *fmt, ...) PRINTF_LIKE(3, 4);
void info(const char *fmt, ...) PRINTF_LIKE(1, 2);
void good(const char *fmt, ...) PRINTF_LIKE(1, 2);
void warn(const char *fmt, ...) PRINTF_LIKE(1, 2);
void oops(const char *fmt, ...) PRINTF_LIKE(1, 2);
void row(const char *label, const char *fmt, ...) PRINTF_LIKE(2, 3);

void spin(const char *label);   /* call repeatedly; draws a bouncing music note */
void spin_done(void);
void nap(int ms);               /* tiny dramatic pause (skipped with --fast) */

void banner(void);
int  ask(const char *prompt, char *buf, int cap);
void wait_enter(const char *prompt);
int  key_pressed(void);         /* non-blocking: was a key hit? (consumes it) */
int  ui_owns_console(void);     /* launched by double-click => window closes on exit */

const char *u8(const wchar_t *w);   /* wide -> UTF-8, rotating static buffers */

#endif
