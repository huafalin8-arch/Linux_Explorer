#include "monitor.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>

/* ---------- tools ---------- */

static void put_cell(Screen *s, int x, int y, const char *text, int width, int fg)
{
    int used = screen_puts(s, x, y, text, width, fg);
    for (int i = used; i < width; i++)
        screen_put(s, x + i, y, " ", fg);
}

static void draw_bar(Screen *s, int x, int y, int width, double pct, int fg)
{
    if (width < 3) return;
    int inner = width - 2;
    int fill = (int)(pct / 100.0 * inner + 0.5);
    if (fill < 0) fill = 0;
    if (fill > inner) fill = inner;

    screen_put(s, x, y, "[", fg);
    for (int i = 0; i < inner; i++)
        screen_put(s, x + 1 + i, y, i < fill ? "█" : "░", fg);
    screen_put(s, x + width - 1, y, "]", fg);
}

/* ---------- Outer frame ---------- */

static void draw_frame(Screen *s, int W, int H, int split)
{
    int fg = FG_CYAN;

    screen_put(s, 0, 0, "╔", fg);
    for (int x = 1; x < W - 1; x++) screen_put(s, x, 0, "═", fg);
    screen_put(s, W - 1, 0, "╗", fg);
    screen_put(s, split, 0, "╦", fg);

    screen_put(s, 0, H - 2, "╚", fg);
    for (int x = 1; x < W - 1; x++) screen_put(s, x, H - 2, "═", fg);
    screen_put(s, W - 1, H - 2, "╝", fg);
    screen_put(s, split, H - 2, "╩", fg);

    for (int y = 1; y < H - 2; y++) {
        screen_put(s, 0, y, "║", fg);
        screen_put(s, W - 1, y, "║", fg);
        screen_put(s, split, y, "║", fg);
    }
}

/* ---------- Left: System Information ---------- */

static void draw_system(Screen *s, Monitor *m, int x, int y, int w, int h)
{
    if (w < 10 || h < 3) return;

    char line[128];
    int cy = y;
    int bottom = y + h;

    /* ---------- CPU 总览 ---------- */
    if (m->cpu.total_usage_available) {
        snprintf(line, sizeof line, "CPU %5.1f%%  %u cores",
                 m->cpu.total_usage, m->cpu.core_count);
    } else {
        snprintf(line, sizeof line, "CPU  N/A    %u cores",
                 m->cpu.core_count);
    }
    put_cell(s, x, cy, line, w, FG_WHITE);
    cy++;

    if (cy < bottom && m->cpu.total_usage_available)
        draw_bar(s, x, cy, w, m->cpu.total_usage, FG_GREEN);
    cy++;

    /* ---------- 每核心：频率 + 使用率 ---------- */
    for (unsigned int i = 0; i < m->cpu.core_count && cy < bottom; i++) {
        const CPUCore *c = &m->cpu.cores[i];

        /* 左：C<n> */
        char tag[8];
        snprintf(tag, sizeof tag, "C%-2u", i);
        put_cell(s, x, cy, tag, 3, FG_GRAY);

        int cx = x + 3;

        /* 中：频率 */
        if (m->cpu.freq_available) {
            char fbuf[24];
            format_freq(fbuf, sizeof fbuf, c->freq_khz);
            put_cell(s, cx, cy, fbuf, 9, FG_CYAN);
            cx += 9;
        }

        /* 右：使用率 */
        char ubuf[24];
        if (c->usage_available) {
            snprintf(ubuf, sizeof ubuf, "%5.1f%%", c->usage);
            put_cell(s, cx, cy, ubuf, 6, FG_WHITE);
            cx += 6;
            /* 剩余给进度条 */
            int remaining = w - (cx - x);
            if (remaining >= 4)
                draw_bar(s, cx, cy, remaining, c->usage, FG_GREEN);
        } else {
            snprintf(ubuf, sizeof ubuf, "  N/A ");
            put_cell(s, cx, cy, ubuf, 6, FG_YELLOW);
        }
        cy++;
    }

    /* ---------- 内存 ---------- */
    if (cy + 1 < bottom) {
        cy++;
        double pct = mem_usage_percent(&m->mem);
        snprintf(line, sizeof line, "MEM %5.1f%%", pct);
        put_cell(s, x, cy, line, w, FG_WHITE);
        cy++;
        if (cy < bottom) draw_bar(s, x, cy, w, pct, FG_MAGENTA);
        cy++;
    }
    if (cy < bottom) {
        char total[24], used[24];
        format_bytes(total, sizeof total, m->mem.total_kb);
        unsigned long long avail = m->mem.available_kb ? m->mem.available_kb : m->mem.free_kb;
        format_bytes(used, sizeof used, m->mem.total_kb > avail ? m->mem.total_kb - avail : 0);
        snprintf(line, sizeof line, "%s / %s", used, total);
        put_cell(s, x, cy, line, w, FG_GRAY);
        cy++;
    }

    /* ---------- Load ---------- */
    if (cy + 1 < bottom) {
        cy++;
        snprintf(line, sizeof line, "LOAD %.2f %.2f %.2f",
                 m->load.load1, m->load.load5, m->load.load15);
        put_cell(s, x, cy, line, w, FG_YELLOW);
        cy++;
    }

    /* ---------- Net ---------- */
    if (cy < bottom) {
        char rx[24], tx[24];
        format_rate(rx, sizeof rx, m->net.rx_rate);
        format_rate(tx, sizeof tx, m->net.tx_rate);
        snprintf(line, sizeof line, "NET ↓%s ↑%s", rx, tx);
        put_cell(s, x, cy, line, w, FG_GREEN);
    }
}

/* ---------- Right: Process list ---------- */

static void draw_processes(Screen *s, Monitor *m, int x, int y, int w, int h)
{
    if (w < 20 || h < 3) return;

    char line[128];
    snprintf(line, sizeof line, "%-7s %-16s %6s %8s", "PID", "NAME", "CPU%", "MEM");
    put_cell(s, x, y, line, w, FG_WHITE | ATTR_REV);
    int cy = y + 1;

    for (int i = 0; i < w; i++) screen_put(s, x + i, cy, "─", FG_GRAY);
    cy++;

    int visible = y + h - cy;
    if (visible <= 0) return;

    int total = proc_filtered_count(m);
    if (total == 0) {
        m->ui.selected = 0;
        m->ui.scroll = 0;
    } else {
        if (m->ui.selected >= total) m->ui.selected = total - 1;
        if (m->ui.selected < 0) m->ui.selected = 0;
        if (m->ui.selected < m->ui.scroll) m->ui.scroll = m->ui.selected;
        if (m->ui.selected >= m->ui.scroll + visible)
            m->ui.scroll = m->ui.selected - visible + 1;
    }

    int fidx = 0, drawn = 0;
    for (size_t i = 0; i < m->procs.count && drawn < visible; i++) {
        ProcInfo *p = &m->procs.items[i];
        if (!proc_matches(m, p)) continue;
        if (fidx < m->ui.scroll) { fidx++; continue; }

        char pidbuf[16], cpubuf[16], membuf[16];
        snprintf(pidbuf, sizeof pidbuf, "%d", p->pid);
        snprintf(cpubuf, sizeof cpubuf, "%.1f", p->cpu_percent);
        format_bytes_short(membuf, sizeof membuf, p->rss_kb);

        int fg = (fidx == m->ui.selected) ? (FG_WHITE | ATTR_REV) : FG_DEFAULT;
        int cx = x;
        put_cell(s, cx, cy, pidbuf, 7, fg); cx += 7;
        put_cell(s, cx, cy, p->name, 16, fg); cx += 16;
        put_cell(s, cx, cy, cpubuf, 6, fg); cx += 6;
        put_cell(s, cx, cy, membuf, w - (cx - x), fg);

        fidx++;
        drawn++;
        cy++;
    }
}

/* ---------- Bottom bar or help ---------- */

static void draw_status(Screen *s, Monitor *m, int y, int W)
{
    char line[256];
    if (m->ui.searching) {
        snprintf(line, sizeof line, " Search: %s_   [Enter] OK   [Esc] Cancel",
                 m->ui.search);
    } else {
        snprintf(line, sizeof line,
                 " [Q]uit [P]ause [R]efresh [C]PU [M]em [/]Search [?]Help  "
                 "%s%s  %.1fs",
                 m->ui.paused ? "PAUSED " : "",
                 m->ui.sort_mode == SORT_CPU ? "CPU" : "MEM",
                 m->cfg.interval);
    }
    for (int i = 0; i < W; i++)
        screen_put(s, i, y, " ", FG_WHITE | ATTR_REV);
    screen_puts(s, 0, y, line, W, FG_WHITE | ATTR_REV);
}

static void draw_help(Screen *s, int W, int H)
{
    static const char *lines[] = {
        "Q         Quit",
        "P         Pause / resume",
        "R         Refresh now",
        "C         Sort by CPU",
        "M         Sort by memory",
        "/         Search process",
        "Up/Down   Select process",
        "?         Toggle this help",
        "Ctrl+C    Quit",
    };
    int bw = 44, bh = (int)(sizeof lines / sizeof lines[0]) + 2;
    int x = (W - bw) / 2, y = (H - bh) / 2;
    if (x < 0 || y < 0) return;

    screen_put(s, x, y, "╔", FG_YELLOW);
    for (int i = 1; i < bw - 1; i++) screen_put(s, x + i, y, "═", FG_YELLOW);
    screen_put(s, x + bw - 1, y, "╗", FG_YELLOW);
    screen_put(s, x, y + bh - 1, "╚", FG_YELLOW);
    for (int i = 1; i < bw - 1; i++) screen_put(s, x + i, y + bh - 1, "═", FG_YELLOW);
    screen_put(s, x + bw - 1, y + bh - 1, "╝", FG_YELLOW);
    for (int j = 1; j < bh - 1; j++) {
        screen_put(s, x, y + j, "║", FG_YELLOW);
        screen_put(s, x + bw - 1, y + j, "║", FG_YELLOW);
    }
    screen_puts(s, x + 3, y, " HELP ", bw - 6, FG_YELLOW);

    int n = (int)(sizeof lines / sizeof lines[0]);
    for (int i = 0; i < n; i++)
        screen_puts(s, x + 2, y + 1 + i, lines[i], bw - 4, FG_WHITE);
}

/* ---------- Entrance ---------- */

void layout_render(Terminal *t, Monitor *m)
{
    Screen *s = &t->screen;
    int W = s->w, H = s->h;
    screen_clear(s);

    if (W < 40 || H < 10) {
        screen_puts(s, 0, 0, "terminal too small", W, FG_RED);
        terminal_present(t);
        return;
    }

    int split = W * 2 / 5;
    if (split < 30) split = 30;
    if (split > W - 30) split = W - 30;

    draw_frame(s, W, H, split);
    screen_puts(s, 3, 0, " SYSTEM ", split - 5, FG_CYAN);
    screen_puts(s, split + 3, 0, " PROCESSES ", W - split - 5, FG_CYAN);

    draw_system(s, m, 2, 1, split - 4, H - 3);
    draw_processes(s, m, split + 2, 1, W - split - 4, H - 3);
    draw_status(s, m, H - 1, W);

    if (m->ui.show_help) draw_help(s, W, H);

    terminal_present(t);
}