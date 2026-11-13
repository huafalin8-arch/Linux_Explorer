#include "monitor.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>

void format_bytes(char *buf, size_t n, unsigned long long kb)
{
    if (kb >= 1024ULL * 1024 * 1024)
        snprintf(buf, n, "%.1f TiB", (double)kb / (1024.0 * 1024 * 1024));
    else if (kb >= 1024ULL * 1024)
        snprintf(buf, n, "%.1f GiB", (double)kb / (1024.0 * 1024));
    else if (kb >= 1024)
        snprintf(buf, n, "%.1f MiB", (double)kb / 1024.0);
    else
        snprintf(buf, n, "%llu KiB", kb);
}

void format_bytes_short(char *buf, size_t n, unsigned long long kb)
{
    if (kb >= 1024ULL * 1024)
        snprintf(buf, n, "%.1fG", (double)kb / (1024.0 * 1024));
    else if (kb >= 1024)
        snprintf(buf, n, "%.1fM", (double)kb / 1024.0);
    else
        snprintf(buf, n, "%lluK", kb);
}

void format_rate(char *buf, size_t n, double bps)
{
    if (bps >= 1024.0 * 1024 * 1024)
        snprintf(buf, n, "%.1f GB/s", bps / (1024.0 * 1024 * 1024));
    else if (bps >= 1024.0 * 1024)
        snprintf(buf, n, "%.1f MB/s", bps / (1024.0 * 1024));
    else if (bps >= 1024.0)
        snprintf(buf, n, "%.1f KB/s", bps / 1024.0);
    else
        snprintf(buf, n, "%.0f B/s", bps);
}

void format_percent(char *buf, size_t n, double pct)
{
    snprintf(buf, n, "%.1f%%", pct);
}

void format_freq(char *buf, size_t n, unsigned long long khz)
{
    if (khz == 0) {
        snprintf(buf, n, "   --   ");
        return;
    }
    double ghz = (double)khz / 1e6;
    if (ghz >= 1.0)
        snprintf(buf, n, "%.2f GHz", ghz);
    else
        snprintf(buf, n, "%.0f MHz", (double)khz / 1e3);
}

int text_width(const char *s)
{
    int w = 0;
    mbstate_t st;
    memset(&st, 0, sizeof st);
    while (*s) {
        wchar_t wc;
        size_t n = mbrtowc(&wc, s, MB_CUR_MAX, &st);
        if (n == (size_t)-1 || n == (size_t)-2) {
            memset(&st, 0, sizeof st);
            s++;
            w++;
            continue;
        }
        if (n == 0) break;
        int cw = wcwidth(wc);
        if (cw > 0) w += cw;
        s += n;
    }
    return w;
}

/* Needs attention */
/* 
Added #include <string.h>: The memset call in text_width requires
* its declaration (in the previous version
* it could still compile because monitor.h indirectly included it
* but I’m not sure about that—please decide for yourselves)
*/