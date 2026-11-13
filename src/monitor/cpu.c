#include "system_info.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static unsigned long long read_freq_khz(unsigned int core_id)
{
    char path[160];
    unsigned long long v = 0;
    FILE *f;

    snprintf(path, sizeof path,
        "/sys/devices/system/cpu/cpu%u/cpufreq/scaling_cur_freq", core_id);
    if ((f = fopen(path, "r"))) {
        if (fscanf(f, "%llu", &v) == 1 && v > 0) { fclose(f); return v; }
        fclose(f);
    }

    snprintf(path, sizeof path,
        "/sys/devices/system/cpu/cpu%u/cpufreq/cpuinfo_cur_freq", core_id);
    if ((f = fopen(path, "r"))) {
        if (fscanf(f, "%llu", &v) == 1 && v > 0) { fclose(f); return v; }
        fclose(f);
    }
    return 0;
}

static void read_proc_cpuinfo_mhz(CPUInfo *cpu)
{
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) return;

    char line[512];
    unsigned int cur = 0;
    bool any = false;

    while (fgets(line, sizeof line, f)) {
        unsigned int proc;
        double mhz;
        if (sscanf(line, "processor : %u", &proc) == 1) {
            cur = proc;
        } else if (sscanf(line, "cpu MHz : %lf", &mhz) == 1) {
            if (cur < cpu->core_count) {
                if (cpu->cores[cur].freq_khz == 0) {
                    cpu->cores[cur].freq_khz =
                        (unsigned long long)(mhz * 1000.0 + 0.5);
                    if (mhz > 0) any = true;
                }
            }
        }
    }
    fclose(f);
    if (any) cpu->freq_available = true;
}

int cpu_info_init(CPUInfo *cpu)
{
    memset(cpu, 0, sizeof *cpu);

    FILE *f = fopen("/proc/stat", "r");
    if (!f) return -1;

    unsigned int n = 0;
    char line[512];
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "cpu", 3) != 0) break;
        if (isdigit((unsigned char)line[3])) n++;
    }
    fclose(f);

    if (n == 0) n = 1;
    cpu->cores = calloc(n, sizeof(CPUCore));
    if (!cpu->cores) return -1;
    cpu->core_count = n;

    cpu->total_usage_available = true;
    for (unsigned int i = 0; i < n; i++) {
        cpu->cores[i].core_id = i;
        cpu->cores[i].usage_available = true;
    }
    return 0;
}

void cpu_info_free(CPUInfo *cpu)
{
    free(cpu->cores);
    cpu->cores = NULL;
    cpu->core_count = 0;
}

static void update_usage(double *usage, bool *available, int *stale,
                         unsigned long long total, unsigned long long idle,
                         unsigned long long *prev_total,
                         unsigned long long *prev_idle,
                         bool *has_prev)
{
    if (*has_prev) {
        unsigned long long dt = total - *prev_total;
        unsigned long long di = idle - *prev_idle;
        if (dt > 0) {
            *usage = (double)(dt - di) * 100.0 / (double)dt;
            *available = true;
            *stale = 0;
        } else {
            (*stale)++;
            if (*stale >= 3) *available = false;
        }
    }
    *prev_total = total;
    *prev_idle = idle;
    *has_prev = true;
}

int cpu_info_sample(CPUInfo *cpu)
{
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return -1;

    char line[512];
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "cpu", 3) != 0) break;

        unsigned long long u, ni, sy, id, io, ir, sq, st;

        if (line[3] == ' ') {
            if (sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                       &u, &ni, &sy, &id, &io, &ir, &sq, &st) < 4)
                continue;
            unsigned long long total = u + ni + sy + id + io + ir + sq + st;
            unsigned long long idle  = id + io;

            update_usage(&cpu->total_usage, &cpu->total_usage_available,
                         &cpu->stale_count,
                         total, idle,
                         &cpu->prev_total, &cpu->prev_idle, &cpu->has_prev);
        } else {
            unsigned int cid;
            if (sscanf(line, "cpu%u %llu %llu %llu %llu %llu %llu %llu %llu",
                       &cid, &u, &ni, &sy, &id, &io, &ir, &sq, &st) < 5)
                continue;

            if (cid >= cpu->core_count) {
                unsigned int nn = cid + 1;
                CPUCore *p = realloc(cpu->cores, nn * sizeof(CPUCore));
                if (!p) continue;
                memset(p + cpu->core_count, 0,
                       (nn - cpu->core_count) * sizeof(CPUCore));
                for (unsigned int k = cpu->core_count; k < nn; k++) {
                    p[k].core_id = k;
                    p[k].usage_available = true;
                }
                cpu->cores = p;
                cpu->core_count = nn;
            }

            CPUCore *c = &cpu->cores[cid];
            unsigned long long total = u + ni + sy + id + io + ir + sq + st;
            unsigned long long idle  = id + io;

            update_usage(&c->usage, &c->usage_available, &c->stale_count,
                         total, idle,
                         &c->prev_total, &c->prev_idle, &c->has_prev);
        }
    }
    fclose(f);

    for (unsigned int i = 0; i < cpu->core_count; i++) {
        unsigned long long khz = read_freq_khz(i);
        if (khz > 0) {
            cpu->cores[i].freq_khz = khz;
            cpu->freq_available = true;
        } else {
            cpu->cores[i].freq_khz = 0;
        }
    }
    if (!cpu->freq_available) read_proc_cpuinfo_mhz(cpu);

    return 0;
}