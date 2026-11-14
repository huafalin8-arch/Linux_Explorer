#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#include <stdbool.h>

/*
* system_info.h
* 
* Provides sampling interfaces for four types of metrics: CPU, memory, load, and network.
* Callers must invoke the *_info_sample() family of functions at a fixed interval.
* Internally, the module calculates usage and rates using the "previous sampling snapshot + difference".
* 
* Data sources: /proc/stat, /proc/cpuinfo, /proc/meminfo,
* 
* 
* Usage conventions:
* cpu_info_init() and cpu_info_free() must be called in pairs;
* On the first call to sample, only a baseline is established; usage/rate may be unavailable,
* 
* Network sampling requires the current time (in seconds) to be passed in externally.
* 
*/
/*
* Last edited: 2026.11.14 09:25 GMT +08:00
* from: huafalin8-arch
*/

/* ---------- CPU ---------- */

typedef struct {
    unsigned int core_id;           /* Logical core number */

    double usage;                   /* 0.0 ~ 100.0 */
    bool   usage_available;         /* whether usage is valid */
    int    stale_count;             /* consecutive dt == 0 count */

    unsigned long long freq_khz;    /* current freq in kHz (0 = unknown) */

    unsigned long long prev_idle;
    unsigned long long prev_total;
    bool has_prev;
} CPUCore;

typedef struct {
    double total_usage;             /* Overall CPU usage */
    bool   total_usage_available;   /* whether total usage is valid */
    int    stale_count;             /* consecutive dt == 0 count for total */

    unsigned int core_count;
    CPUCore *cores;                 /* Information per core, with a length of core_count */

    bool   freq_available;          /* true if any core's frequency is readable */

    unsigned long long prev_idle;   /* For overall sampling */
    unsigned long long prev_total;
    bool has_prev;
} CPUInfo;

int  cpu_info_init(CPUInfo *cpu);   /* Allocated cores, returned 0 on success */
void cpu_info_free(CPUInfo *cpu);
int  cpu_info_sample(CPUInfo *cpu); /* Sample once, update usage */

/* ---------- Memory ---------- */

typedef struct {
    unsigned long long total_kb, free_kb, available_kb;
    unsigned long long buffers_kb, cached_kb;
    unsigned long long swap_total_kb, swap_free_kb;
} MemInfo;

int    mem_info_sample(MemInfo *mem);
double mem_usage_percent(const MemInfo *m);
double swap_usage_percent(const MemInfo *m);

/* ---------- Load Average ---------- */

typedef struct {
    double load1, load5, load15;    /* 1/5/15-minute average load */
    unsigned long runnable;         /* Number of runnable processes */
    unsigned long total;            /* Total number of processes/threads */
    unsigned long last_pid;
} LoadInfo;

void load_info_sample(LoadInfo *load);

/* ---------- Network ---------- */

typedef struct {
    unsigned long long rx_bytes, tx_bytes;
    double rx_rate, tx_rate;        /* byte/s */
    unsigned long long prev_rx, prev_tx;
    double prev_time;               /* Last sampling time (seconds) */
    bool has_prev;
} NetInfo;

void net_info_sample(NetInfo *net, double now);

#endif