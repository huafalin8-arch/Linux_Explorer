#ifndef MONITOR_H
#define MONITOR_H

#include <stdbool.h>
#include <stddef.h>
#include "system_info.h"
#include "process_info.h"
#include "terminal.h"

typedef struct {
    double interval;
    int    refresh_fps;
    bool   no_color;
    bool   once;
} Config;

typedef enum { SORT_CPU, SORT_MEM } SortMode;

typedef struct {
    bool paused;
    bool show_help;
    bool searching;
    SortMode sort_mode;
    int  selected;      /* Filter the indices in the sequence */
    int  scroll;
    char search[64];
    size_t search_len;
} UIState;

typedef struct {
    Config cfg;
    CPUInfo cpu;
    MemInfo mem;
    LoadInfo load;
    NetInfo net;
    ProcessList procs;
    UIState ui;
    double start_time;
} Monitor;

int  monitor_init(Monitor *m, const Config *cfg);
void monitor_shutdown(Monitor *m);
void monitor_sample(Monitor *m);
int  monitor_run(Monitor *m, Terminal *t);
void monitor_render_once(Monitor *m);

int  proc_matches(const Monitor *m, const ProcInfo *p);
int  proc_filtered_count(const Monitor *m);

/* Signal coordination */
void monitor_request_quit(void);
void monitor_request_resize(void);

/* layout.c */
void layout_render(Terminal *t, Monitor *m);

/* utils/format.c */
void format_bytes(char *buf, size_t n, unsigned long long kb);
void format_bytes_short(char *buf, size_t n, unsigned long long kb);
void format_rate(char *buf, size_t n, double bytes_per_sec);
void format_percent(char *buf, size_t n, double pct);
int  text_width(const char *s);

/* utils/timer.c */
double timer_now(void);
void   timer_sleep(double sec);

#endif