#ifndef PROCESS_INFO_H
#define PROCESS_INFO_H

#include <sys/types.h>
#include <stddef.h>
#include <stdbool.h>

/*
* process_info.h - Linux process information collection module
* 
* Purpose:
* Collect runtime information for each process in the system from the /proc
* filesystem, and calculate CPU / memory usage percentages for process
* monitoring (top/ps-like) use.
* 
* Main data structures:
* ProcInfo     : Sampling result for a single process (pid, name, state,
* 
* PrevCache    : Cache based on hash buckets + object pool, storing the
* 
* 
* 
* ProcessList  : Process list container, holding sampling results, cache,
* 
* 
* Interfaces:
* proc_list_init()   Initialize the list and cache
* proc_list_free()   Release all resources
* proc_list_sample() Perform one sampling; total_mem_kb is used to calculate
* 
* 
* proc_list_sort()   Sort the results (by memory or other fields)
*/

/*
* Last edited: 2026.11.14 09:12 GMT +00:08
* from: huafalin8-arch
*/

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