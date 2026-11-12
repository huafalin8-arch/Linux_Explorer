#ifndef PROCESS_INFO_H
#define PROCESS_INFO_H

#include <sys/types.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    pid_t pid;
    char  name[64];
    char  state;
    unsigned long long cpu_jiffies;
    double cpu_percent;
    unsigned long long rss_kb;
    double mem_percent;
    unsigned int threads;
    int nice;
} ProcInfo;

#define PREV_BUCKETS 4096

typedef struct PrevNode {
    pid_t pid;
    unsigned long long cpu_jiffies;
    struct PrevNode *next;
} PrevNode;

typedef struct {
    PrevNode *buckets[PREV_BUCKETS];
    PrevNode *pool;
    size_t pool_cap, pool_used;
} PrevCache;

typedef struct {
    ProcInfo *items;
    size_t count, cap;
    PrevCache cache;
    double prev_time;
    bool has_prev;
} ProcessList;

void proc_list_init(ProcessList *pl);
void proc_list_free(ProcessList *pl);
int  proc_list_sample(ProcessList *pl, unsigned long long total_mem_kb, double now);
void proc_list_sort(ProcessList *pl, int by_mem);

#endif