#include "monitor.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>

static volatile sig_atomic_t g_quit = 0;
static volatile sig_atomic_t g_resized = 0;

void monitor_request_quit(void)   { g_quit = 1; }
void monitor_request_resize(void) { g_resized = 1; }

int monitor_init(Monitor *m, const Config *cfg)
{
    memset(m, 0, sizeof *m);
    m->cfg = *cfg;
    m->start_time = timer_now();
    if (cpu_info_init(&m->cpu) != 0) return -1;
    mem_info_sample(&m->mem);
    load_info_sample(&m->load);
    proc_list_init(&m->procs);
    m->ui.sort_mode = SORT_CPU;
    return 0;
}

void monitor_shutdown(Monitor *m)
{
    cpu_info_free(&m->cpu);
    proc_list_free(&m->procs);
}

void monitor_sample(Monitor *m)
{
    double now = timer_now();
    cpu_info_sample(&m->cpu);
    mem_info_sample(&m->mem);
    load_info_sample(&m->load);
    net_info_sample(&m->net, now);
    proc_list_sample(&m->procs, m->mem.total_kb, now);
    proc_list_sort(&m->procs, m->ui.sort_mode == SORT_MEM);
}

int proc_matches(const Monitor *m, const ProcInfo *p)
{
    return m->ui.search[0] == 0 || strstr(p->name, m->ui.search) != NULL;
}

int proc_filtered_count(const Monitor *m)
{
    int c = 0;
    for (size_t i = 0; i < m->procs.count; i++)
        if (proc_matches(m, &m->procs.items[i])) c++;
    return c;
}

static int handle_key(Monitor *m, int key)
{
    if (m->ui.searching) {
        if (key == KEY_ENTER) {
            m->ui.searching = false;
            m->ui.selected = 0;
            m->ui.scroll = 0;
        } else if (key == KEY_ESC) {
            m->ui.searching = false;
            m->ui.search[0] = 0;
            m->ui.search_len = 0;
            m->ui.selected = 0;
            m->ui.scroll = 0;
        } else if (key == KEY_BACKSPACE) {
            if (m->ui.search_len > 0) {
                m->ui.search[--m->ui.search_len] = 0;
                m->ui.selected = 0;
                m->ui.scroll = 0;
            }
        } else if (key >= 32 && key < 127 &&
                   m->ui.search_len + 1 < sizeof m->ui.search) {
            m->ui.search[m->ui.search_len++] = (char)key;
            m->ui.search[m->ui.search_len] = 0;
            m->ui.selected = 0;
            m->ui.scroll = 0;
        }
        return 0;
    }

    switch (key) {
    case 'q': case 'Q': return 1;
    case 'p': case 'P': m->ui.paused = !m->ui.paused; break;
    case 'r': case 'R': monitor_sample(m); break;
    case 'c': case 'C':
        m->ui.sort_mode = SORT_CPU;
        proc_list_sort(&m->procs, 0);
        m->ui.selected = m->ui.scroll = 0;
        break;
    case 'm': case 'M':
        m->ui.sort_mode = SORT_MEM;
        proc_list_sort(&m->procs, 1);
        m->ui.selected = m->ui.scroll = 0;
        break;
    case '/':
        m->ui.searching = true;
        m->ui.search_len = 0;
        m->ui.search[0] = 0;
        break;
    case '?':
        m->ui.show_help = !m->ui.show_help;
        break;
    case KEY_UP:
        if (m->ui.selected > 0) m->ui.selected--;
        break;
    case KEY_DOWN:
        m->ui.selected++;
        break;
    }
    return 0;
}

int monitor_run(Monitor *m, Terminal *t)
{
    double last_sample = timer_now();
    double last_frame = 0;
    double frame_dt = 1.0 / (m->cfg.refresh_fps > 0 ? m->cfg.refresh_fps : 10);

    while (!g_quit) {
        double now = timer_now();

        if (!m->ui.paused && now - last_sample >= m->cfg.interval) {
            monitor_sample(m);
            last_sample = now;
        }

        if (g_resized) {
            terminal_get_size(t);
            terminal_clear(t);
            g_resized = 0;
            last_frame = 0;
        }

        if (now - last_frame >= frame_dt) {
            layout_render(t, m);
            last_frame = now;
        }

        double wait = last_frame + frame_dt - timer_now();
        if (wait < 0) wait = 0;
        int timeout = (int)(wait * 1000);
        if (timeout > 50) timeout = 50;   /* Ensure sampling and signal response. */

        int key = terminal_poll_key(t, timeout);
        if (key != KEY_NONE) {
            if (handle_key(m, key)) break;
            while ((key = terminal_poll_key(t, 0)) != KEY_NONE) {
                if (handle_key(m, key)) { g_quit = 1; break; }
            }
        }
    }
    return 0;
}

void monitor_render_once(Monitor *m)
{
    printf("TermScope report\n");
    printf("CPU      : %5.1f%%  (%u cores)\n",
           m->cpu.total_usage, m->cpu.core_count);
    for (unsigned int i = 0; i < m->cpu.core_count; i++)
        printf("  Core %-2u: %5.1f%%\n", i, m->cpu.cores[i].usage);

    char tbuf[24], ubuf[24];
    format_bytes(tbuf, sizeof tbuf, m->mem.total_kb);
    unsigned long long avail = m->mem.available_kb ? m->mem.available_kb : m->mem.free_kb;
    format_bytes(ubuf, sizeof ubuf, m->mem.total_kb > avail ? m->mem.total_kb - avail : 0);
    printf("Memory   : %5.1f%%  (%s / %s)\n",
           mem_usage_percent(&m->mem), ubuf, tbuf);
    printf("Swap     : %5.1f%%\n", swap_usage_percent(&m->mem));
    printf("Load     : %.2f %.2f %.2f\n",
           m->load.load1, m->load.load5, m->load.load15);

    printf("\nTop processes by %s:\n",
           m->ui.sort_mode == SORT_CPU ? "CPU" : "memory");
    printf("%-8s %-20s %8s %10s\n", "PID", "NAME", "CPU%", "MEM");
    int shown = 0;
    for (size_t i = 0; i < m->procs.count && shown < 20; i++, shown++) {
        ProcInfo *p = &m->procs.items[i];
        char mb[16];
        format_bytes_short(mb, sizeof mb, p->rss_kb);
        printf("%-8d %-20.20s %7.1f%% %10s\n",
               p->pid, p->name, p->cpu_percent, mb);
    }
}