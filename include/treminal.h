#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdbool.h>
#include <stddef.h>

#define KEY_NONE      (-1)
#define KEY_ESC       27
#define KEY_ENTER     13
#define KEY_BACKSPACE 127
#define KEY_UP        1001
#define KEY_DOWN      1002
#define KEY_LEFT      1003
#define KEY_RIGHT     1004

enum {
    FG_DEFAULT = 0,
    FG_BLACK   = 30,
    FG_RED     = 31,
    FG_GREEN   = 32,
    FG_YELLOW  = 33,
    FG_BLUE    = 34,
    FG_MAGENTA = 35,
    FG_CYAN    = 36,
    FG_WHITE   = 37,
    FG_GRAY    = 90,
    ATTR_REV   = 0x80,
};

typedef struct {
    char ch[4];        /* UTF-8 + NUL，At most one display character */
    unsigned char fg;
} Cell;

typedef struct {
    int w, h;
    Cell *cells;
} Screen;

typedef struct {
    int fd_in, fd_out;
    bool raw_enabled;
    bool no_color;
    int width, height;
    char *out;
    size_t out_len, out_cap;
    Screen screen;
} Terminal;

int  terminal_init(Terminal *t, bool no_color);
void terminal_shutdown(Terminal *t);
void terminal_get_size(Terminal *t);
void terminal_clear(Terminal *t);
void terminal_hide_cursor(Terminal *t);
void terminal_show_cursor(Terminal *t);
int  terminal_poll_key(Terminal *t, int timeout_ms);
void terminal_present(Terminal *t);   /* flush screen + out buffer */

/* Screen API —— layout.c use */
void screen_resize(Screen *s, int w, int h);
void screen_free(Screen *s);
void screen_clear(Screen *s);
void screen_put(Screen *s, int x, int y, const char *ch, int fg);
int  screen_puts(Screen *s, int x, int y, const char *text, int max_w, int fg);

#endif