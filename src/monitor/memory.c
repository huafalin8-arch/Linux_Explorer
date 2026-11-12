#include "system_info.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int mem_info_sample(MemInfo *mem)
{
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1;

    memset(mem, 0, sizeof *mem);
    char *line = NULL;
    size_t cap = 0;
    while (getline(&line, &cap, f) > 0) {
        unsigned long long v = 0;
        if      (sscanf(line, "MemTotal: %llu", &v) == 1)     mem->total_kb = v;
        else if (sscanf(line, "MemFree: %llu", &v) == 1)      mem->free_kb = v;
        else if (sscanf(line, "MemAvailable: %llu", &v) == 1) mem->available_kb = v;
        else if (sscanf(line, "Buffers: %llu", &v) == 1)      mem->buffers_kb = v;
        else if (sscanf(line, "Cached: %llu", &v) == 1)       mem->cached_kb = v;
        else if (sscanf(line, "SwapTotal: %llu", &v) == 1)    mem->swap_total_kb = v;
        else if (sscanf(line, "SwapFree: %llu", &v) == 1)     mem->swap_free_kb = v;
    }
    free(line);
    fclose(f);
    return 0;
}

double mem_usage_percent(const MemInfo *m)
{
    if (!m->total_kb) return 0.0;
    unsigned long long avail = m->available_kb ? m->available_kb : m->free_kb;
    if (avail > m->total_kb) avail = m->total_kb;
    return (double)(m->total_kb - avail) * 100.0 / (double)m->total_kb;
}

double swap_usage_percent(const MemInfo *m)
{
    if (!m->swap_total_kb) return 0.0;
    return (double)(m->swap_total_kb - m->swap_free_kb) * 100.0 / (double)m->swap_total_kb;
}

void load_info_sample(LoadInfo *load)
{
    FILE *f = fopen("/proc/loadavg", "r");
    if (!f) return;
    fscanf(f, "%lf %lf %lf %lu/%lu %lu",
           &load->load1, &load->load5, &load->load15,
           &load->runnable, &load->total, &load->last_pid);
    fclose(f);
}

void net_info_sample(NetInfo *net, double now)
{
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) return;

    unsigned long long rx = 0, tx = 0;
    char line[512];
    int lineno = 0;
    while (fgets(line, sizeof line, f)) {
        if (++lineno <= 2) continue;              /* Skip header */
        char *colon = strchr(line, ':');
        if (!colon) continue;
        if (strncmp(line, "  lo:", 5) == 0) continue;   /* skip loopback */

        unsigned long long v[16];
        int cnt = sscanf(colon + 1,
            "%llu %llu %llu %llu %llu %llu %llu %llu %llu",
            &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8]);
        if (cnt >= 9) { rx += v[0]; tx += v[8]; }
    }
    fclose(f);

    net->rx_bytes = rx;
    net->tx_bytes = tx;

    if (net->has_prev && now > net->prev_time) {
        double dt = now - net->prev_time;
        net->rx_rate = (double)(rx - net->prev_rx) / dt;
        net->tx_rate = (double)(tx - net->prev_tx) / dt;
    }
    net->prev_rx = rx;
    net->prev_tx = tx;
    net->prev_time = now;
    net->has_prev = true;
}