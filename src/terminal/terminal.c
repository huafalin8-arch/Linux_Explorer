#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <termios.h>
#include <poll.h>
#include <errno.h>
#include <wchar.h>
#include <sys/ioctl.h>

static struct termios g_orig;
static int g_raw = 0;

/* ---------- Output buffer ---------- */

static void out_reserve(Terminal *t, size_t n)
{
    if (t->out_len + n <= t->out_cap) return;
    size_t nc = t->out_cap ? t->out_cap : 8192;
    while (nc < t->out_len + n) nc *= 2;
    char *p = realloc(t->out, nc);
    if (!p) return;
    t->out = p;
    t->out_cap = nc;
}

static void out_bytes(Terminal *t, const char *s, size_t n)
{
    out_reserve(t, n);
    memcpy(t->out + t->out_len, s, n);
    t->out_len += n;
}

static void out_str(Terminal *t, const char *s)
{
    out_bytes(t, s, strlen(s));
}

static void out_fmt(Terminal *t, const char *fmt, ...)
{
    char buf[64];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) out_bytes(t, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
}

void terminal_flush_buffer(Terminal *t)
{
    size_t off = 0;
    while (off < t->out_len) {
        ssize_t n = write(t->fd_out, t->out + off, t->out_len - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        off += (size_t)n;
    }
    t->out_len = 0;
}

/* ---------- Screen ---------- */

void screen_resize(Screen *s, int w, int h)
{
    if (s->w == w && s->h == h) return;
    Cell *p = realloc(s->cells, (size_t)w * h * sizeof(Cell));
    if (!p) return;
    s->cells = p;
    s->w = w;
    s->h = h;
    screen_clear(s);
}

void screen_free(Screen *s)
{
    free(s->cells);
    s->cells = NULL;
    s->w = s->h = 0;
}

void screen_clear(Screen *s)
{
    size_t total = (size_t)s->w * s->h;
    for (size_t i = 0; i < total; i++) {
        s->cells[i].ch[0] = ' ';
        s->cells[i].ch[1] = 0;
        s->cells[i].fg = FG_DEFAULT;
    }
}

void screen_put(Screen *s, int x, int y, const char *ch, int fg)
{
    if (x < 0 || x >= s->w || y < 0 || y >= s->h) return;
    Cell *c = &s->cells[(size_t)y * s->w + x];
    memset(c->ch, 0, sizeof c->ch);
    size_t n = strlen(ch);
    if (n >= sizeof c->ch) n = sizeof c->ch - 1;
    memcpy(c->ch, ch, n);
    c->fg = (unsigned char)fg;
}

int screen_puts(Screen *s, int x, int y, const char *text, int max_w, int fg)
{
    if (y < 0 || y >= s->h) return 0;
    mbstate_t st;
    memset(&st, 0, sizeof st);
    int cx = x, used = 0;

    while (*text && used < max_w) {
        wchar_t wc;
        size_t n = mbrtowc(&wc, text, MB_CUR_MAX, &st);
        if (n == (size_t)-1 || n == (size_t)-2) {
            memset(&st, 0, sizeof st);
            text++;
            continue;
        }
        if (n == 0) break;

        int cw = wcwidth(wc);
        if (cw < 0) cw = 0;
        int step = cw > 0 ? cw : 1;
        if (used + step > max_w) break;

        if (cx < s->w) {
            char tmp[4] = {0};
            memcpy(tmp, text, n < 4 ? n : 3);
            screen_put(s, cx, y, tmp, fg);
        }
        if (cw == 2 && cx + 1 < s->w) {
            Cell *c2 = &s->cells[(size_t)y * s->w + (cx + 1)];
            c2->ch[0] = 0;
            c2->fg = (unsigned char)fg;
        }
        cx += step;
        used += step;
        text += n;
    }
    return used;
}

static void screen_present(Screen *s, Terminal *t)
{
    int last_fg = -1;
    out_str(t, "\033[0m");

    for (int y = 0; y < s->h; y++) {
        out_fmt(t, "\033[%d;1H", y + 1);
        for (int x = 0; x < s->w; x++) {
            Cell *c = &s->cells[(size_t)y * s->w + x];
            if (c->ch[0] == 0) continue;

            if (!t->no_color && (unsigned char)c->fg != last_fg) {
                out_str(t, "\033[0m");
                unsigned char f = c->fg;
                if (f & ATTR_REV) out_str(t, "\033[7m");
                int color = f & 0x7f;
                if (color) out_fmt(t, "\033[%dm", color);
                last_fg = (unsigned char)c->fg;
            }
            out_str(t, c->ch);
        }
    }
    if (!t->no_color) out_str(t, "\033[0m");
}

/* ---------- Life cycle ---------- */

int terminal_init(Terminal *t, bool no_color)
{
    memset(t, 0, sizeof *t);
    t->fd_in = STDIN_FILENO;
    t->fd_out = STDOUT_FILENO;
    t->no_color = no_color;

    if (!isatty(t->fd_in) || !isatty(t->fd_out)) return -1;
    if (tcgetattr(t->fd_in, &g_orig) != 0) return -1;

    struct termios raw = g_orig;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= CS8;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);   /* Reserve ISIG */
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(t->fd_in, TCSAFLUSH, &raw) != 0) return -1;
    g_raw = 1;
    t->raw_enabled = true;

    terminal_get_size(t);
    terminal_hide_cursor(t);
    terminal_clear(t);
    out_str(t, "\033[2J\033[H");
    terminal_flush_buffer(t);
    return 0;
}

void terminal_shutdown(Terminal *t)
{
    if (!t->raw_enabled) return;
    terminal_show_cursor(t);
    out_str(t, "\033[0m\033[2J\033[H");
    terminal_flush_buffer(t);
    tcsetattr(t->fd_in, TCSAFLUSH, &g_orig);
    g_raw = 0;
    t->raw_enabled = false;
    screen_free(&t->screen);
    free(t->out);
    t->out = NULL;
    t->out_cap = t->out_len = 0;
}

void terminal_get_size(Terminal *t)
{
    struct winsize ws;
    if (ioctl(t->fd_out, TIOCGWINSZ, &ws) == 0 && ws.ws_col && ws.ws_row) {
        t->width = ws.ws_col;
        t->height = ws.ws_row;
    } else {
        t->width = 80;
        t->height = 24;
    }
    screen_resize(&t->screen, t->width, t->height);
}

void terminal_clear(Terminal *t)
{
    out_str(t, "\033[2J\033[H");
    screen_clear(&t->screen);
}

void terminal_hide_cursor(Terminal *t) { out_str(t, "\033[?25l"); }
void terminal_show_cursor(Terminal *t) { out_str(t, "\033[?25h"); }

void terminal_present(Terminal *t)
{
    screen_present(&t->screen, t);
    terminal_flush_buffer(t);
}

int terminal_poll_key(Terminal *t, int timeout_ms)
{
    struct pollfd pfd = { .fd = t->fd_in, .events = POLLIN };
    int r = poll(&pfd, 1, timeout_ms);
    if (r <= 0 || !(pfd.revents & POLLIN)) return KEY_NONE;

    unsigned char buf[8];
    ssize_t n = read(t->fd_in, buf, sizeof buf);
    if (n <= 0) return KEY_NONE;

    if (buf[0] == 0x1b) {
        if (n >= 3 && buf[1] == '[') {
            switch (buf[2]) {
            case 'A': return KEY_UP;
            case 'B': return KEY_DOWN;
            case 'C': return KEY_RIGHT;
            case 'D': return KEY_LEFT;
            }
        }
        return KEY_ESC;
    }
    if (buf[0] == '\r' || buf[0] == '\n') return KEY_ENTER;
    if (buf[0] == 0x7f || buf[0] == 8)   return KEY_BACKSPACE;
    return buf[0];
}