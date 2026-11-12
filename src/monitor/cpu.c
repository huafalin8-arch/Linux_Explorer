#include "system_info.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
    return 0;
}

void cpu_info_free(CPUInfo *cpu)
{
    free(cpu->cores);
    cpu->cores = NULL;
    cpu->core_count = 0;
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
            unsigned long long idle = id + io;
            if (cpu->has_prev) {
                unsigned long long dt = total - cpu->prev_total;
                unsigned long long di = idle - cpu->prev_idle;
                cpu->total_usage = dt ? (double)(dt - di) * 100.0 / (double)dt : 0.0;
            }
            cpu->prev_total = total;
            cpu->prev_idle = idle;
            cpu->has_prev = true;
        } else {
            unsigned int cid;
            if (sscanf(line, "cpu%u %llu %llu %llu %llu %llu %llu %llu %llu",
                       &cid, &u, &ni, &sy, &id, &io, &ir, &sq, &st) < 5)
                continue;

            if (cid >= cpu->core_count) {
                unsigned int nn = cid + 1;
                CPUCore *p = realloc(cpu->cores, nn * sizeof(CPUCore));
                if (!p) continue;
                memset(p + cpu->core_count, 0, (nn - cpu->core_count) * sizeof(CPUCore));
                cpu->cores = p;
                cpu->core_count = nn;
            }

            CPUCore *c = &cpu->cores[cid];
            c->core_id = cid;
            unsigned long long total = u + ni + sy + id + io + ir + sq + st;
            unsigned long long idle = id + io;
            if (c->has_prev) {
                unsigned long long dt = total - c->prev_total;
                unsigned long long di = idle - c->prev_idle;
                c->usage = dt ? (double)(dt - di) * 100.0 / (double)dt : 0.0;
            }
            c->prev_total = total;
            c->prev_idle = idle;
            c->has_prev = true;
        }
    }
    fclose(f);
    return 0;
}