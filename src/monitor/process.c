#include "process_info.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>

static size_t pid_hash(pid_t pid)
{
    return ((unsigned)pid * 2654435761u) % PREV_BUCKETS;
}

static PrevNode *cache_find(PrevCache *c, pid_t pid)
{
    for (PrevNode *n = c->buckets[pid_hash(pid)]; n; n = n->next)
        if (n->pid == pid) return n;
    return NULL;
}

static void cache_clear(PrevCache *c)
{
    memset(c->buckets, 0, sizeof c->buckets);
    c->pool_used = 0;
}

static int cache_reserve(PrevCache *c, size_t n)
{
    if (c->pool_cap >= n) return 0;

    size_t nc = c->pool_cap ? c->pool_cap : 1024;
    while (nc < n) {
        if (nc > SIZE_MAX / 2) { nc = n; break; }
        nc *= 2;
    }

    PrevNode *np = realloc(c->pool, nc * sizeof(PrevNode));
    if (!np) return -1;

    c->pool = np;
    c->pool_cap = nc;
    return 0;
}

static void cache_insert(PrevCache *c, pid_t pid, unsigned long long j)
{
    if (c->pool_used == c->pool_cap) {
        size_t nc = c->pool_cap ? c->pool_cap * 2 : 1024;
        PrevNode *np = realloc(c->pool, nc * sizeof(PrevNode));
        if (!np) return;
        c->pool = np;
        c->pool_cap = nc;
    }
    PrevNode *n = &c->pool[c->pool_used++];
    n->pid = pid;
    n->cpu_jiffies = j;
    size_t b = pid_hash(pid);
    n->next = c->buckets[b];
    c->buckets[b] = n;
}

void proc_list_init(ProcessList *pl) { memset(pl, 0, sizeof *pl); }

void proc_list_free(ProcessList *pl)
{
    free(pl->items);
    free(pl->cache.pool);
    memset(pl, 0, sizeof *pl);
}

static int parse_stat(pid_t pid, ProcInfo *p, unsigned long long *cpu_j)
{
    char path[64];
    snprintf(path, sizeof path, "/proc/%d/stat", (int)pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char buf[1024];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    if (n == 0) return -1;
    buf[n] = 0;

    char *r = strrchr(buf, ')');
    if (!r || r[1] != ' ') return -1;
    p->state = r[2];

    int ppid, pgrp, sess, tty, tpg;
    unsigned int flags;
    unsigned long minf, cminf, majf, cmajf;
    unsigned long long ut, st;
    long prio, nice, nthr;

    int cnt = sscanf(r + 4,
        "%d %d %d %d %d %u %lu %lu %lu %lu %llu %llu %*lld %*lld %ld %ld %ld",
        &ppid, &pgrp, &sess, &tty, &tpg, &flags,
        &minf, &cminf, &majf, &cmajf, &ut, &st, &prio, &nice, &nthr);
    if (cnt < 15) return -1;

    p->nice = (int)nice;
    p->threads = (unsigned)nthr;
    *cpu_j = ut + st;
    return 0;
}

static void read_comm(pid_t pid, char *out, size_t n)
{
    char path[64];
    snprintf(path, sizeof path, "/proc/%d/comm", (int)pid);
    FILE *f = fopen(path, "r");
    if (!f) { snprintf(out, n, "?"); return; }
    if (!fgets(out, n, f)) out[0] = 0;
    fclose(f);
    size_t len = strlen(out);
    if (len && out[len - 1] == '\n') out[len - 1] = 0;
}

static unsigned long long read_rss_kb(pid_t pid, long page_kb)
{
    char path[64];
    snprintf(path, sizeof path, "/proc/%d/statm", (int)pid);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    unsigned long sz, res;
    unsigned long long kb = 0;
    if (fscanf(f, "%lu %lu", &sz, &res) == 2)
        kb = (unsigned long long)res * (unsigned long long)page_kb;
    fclose(f);
    return kb;
}

int proc_list_sample(ProcessList *pl, unsigned long long total_mem_kb, double now)
{
    DIR *d = opendir("/proc");
    if (!d) return -1;

    long page_kb = sysconf(_SC_PAGESIZE) / 1024;
    long hz = sysconf(_SC_CLK_TCK);
    double dt = pl->has_prev ? now - pl->prev_time : 0.0;

    size_t cnt = 0;
    int oom = 0;
    struct dirent *de;

    while ((de = readdir(d))) {
        if (!isdigit((unsigned char)de->d_name[0])) continue;
        pid_t pid = (pid_t)atoi(de->d_name);
        if (pid <= 0) continue;

        ProcInfo p;
        memset(&p, 0, sizeof p);
        p.pid = pid;

        unsigned long long cj;
        if (parse_stat(pid, &p, &cj) != 0) continue;
        p.cpu_jiffies = cj;
        read_comm(pid, p.name, sizeof p.name);
        p.rss_kb = read_rss_kb(pid, page_kb);
        if (total_mem_kb)
            p.mem_percent = (double)p.rss_kb * 100.0 / (double)total_mem_kb;

        if (pl->has_prev && hz > 0 && dt > 0) {
            PrevNode *prev = cache_find(&pl->cache, pid);
            if (prev && cj >= prev->cpu_jiffies) {
                unsigned long long diff = cj - prev->cpu_jiffies;
                p.cpu_percent = (double)diff / (double)hz / dt * 100.0;
            }
        }

        if (cnt == pl->cap) {
            size_t nc = pl->cap ? pl->cap * 2 : 256;
            ProcInfo *np = realloc(pl->items, nc * sizeof(ProcInfo));
            if (!np) { oom = 1; break; }
            pl->items = np;
            pl->cap = nc;
        }
        pl->items[cnt++] = p;
    }
    closedir(d);

    if (oom) {
        pl->count = cnt;
        return -1;
    }

    if (cache_reserve(&pl->cache, cnt) != 0) {
        pl->count = cnt;
        return -1;
    }

    pl->count = cnt;

    cache_clear(&pl->cache);
    for (size_t i = 0; i < cnt; i++)
        cache_insert(&pl->cache, pl->items[i].pid, pl->items[i].cpu_jiffies);

    pl->prev_time = now;
    pl->has_prev = true;
    return 0;
}

static int cmp_cpu(const void *a, const void *b)
{
    const ProcInfo *x = a, *y = b;
    if (x->cpu_percent < y->cpu_percent) return 1;
    if (x->cpu_percent > y->cpu_percent) return -1;
    return (x->rss_kb < y->rss_kb) - (x->rss_kb > y->rss_kb);
}

static int cmp_mem(const void *a, const void *b)
{
    const ProcInfo *x = a, *y = b;
    if (x->rss_kb < y->rss_kb) return 1;
    if (x->rss_kb > y->rss_kb) return -1;
    return (x->cpu_percent < y->cpu_percent) - (x->cpu_percent > y->cpu_percent);
}

void proc_list_sort(ProcessList *pl, int by_mem)
{
    qsort(pl->items, pl->count, sizeof(ProcInfo), by_mem ? cmp_mem : cmp_cpu);
}