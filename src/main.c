#include "monitor.h"
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <signal.h>
#include <getopt.h>

#define PROGRAM "tscope"
#define VERSION "0.1.0"

static Terminal g_term;
static int g_term_active = 0;

static void restore_terminal(void)
{
    if (g_term_active) {
        terminal_shutdown(&g_term);
        g_term_active = 0;
    }
}

static void on_quit(int sig)   { (void)sig; monitor_request_quit(); }
static void on_winch(int sig)  { (void)sig; monitor_request_resize(); }

static void usage(FILE *out)
{
    fprintf(out,
        "Usage: %s [options]\n"
        "Options:\n"
        "  --interval SEC   Sampling interval in seconds (default 1.0)\n"
        "  --no-color       Disable colors\n"
        "  --once           Print one report and exit\n"
        "  --version        Show version\n"
        "  --help           Show this help\n",
        PROGRAM);
}

int main(int argc, char **argv)
{
    setlocale(LC_CTYPE, "");

    Config cfg = {
        .interval    = 1.0,
        .refresh_fps = 10,
        .no_color    = false,
        .once        = false,
    };

    enum { OPT_VERSION = 1000, OPT_HELP, OPT_INTERVAL, OPT_NO_COLOR, OPT_ONCE };
    static const struct option opts[] = {
        {"version",  no_argument,       0, OPT_VERSION},
        {"help",     no_argument,       0, OPT_HELP},
        {"interval", required_argument, 0, OPT_INTERVAL},
        {"no-color", no_argument,       0, OPT_NO_COLOR},
        {"once",     no_argument,       0, OPT_ONCE},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "", opts, NULL)) != -1) {
        switch (c) {
        case OPT_VERSION:
            printf("%s %s\n", PROGRAM, VERSION);
            return 0;
        case OPT_HELP:
            usage(stdout);
            return 0;
        case OPT_INTERVAL: {
            char *end;
            double v = strtod(optarg, &end);
            if (*end != 0 || v <= 0 || v > 3600) {
                fprintf(stderr, "%s: invalid interval '%s'\n", PROGRAM, optarg);
                return 2;
            }
            cfg.interval = v;
            break;
        }
        case OPT_NO_COLOR: cfg.no_color = true; break;
        case OPT_ONCE:     cfg.once = true; break;
        default:
            usage(stderr);
            return 2;
        }
    }
    if (optind < argc) {
        fprintf(stderr, "%s: unexpected argument '%s'\n", PROGRAM, argv[optind]);
        return 2;
    }

    Monitor m;
    if (monitor_init(&m, &cfg) != 0) {
        fprintf(stderr, "%s: failed to initialize monitor\n", PROGRAM);
        return 1;
    }

    if (cfg.once) {
        monitor_sample(&m);
        timer_sleep(cfg.interval);
        monitor_sample(&m);
        monitor_render_once(&m);
        monitor_shutdown(&m);
        return 0;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = on_quit;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sa.sa_handler = on_winch;
    sigaction(SIGWINCH, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    atexit(restore_terminal);

    if (terminal_init(&g_term, cfg.no_color) != 0) {
        fprintf(stderr, "%s: this program requires an interactive terminal\n", PROGRAM);
        monitor_shutdown(&m);
        return 1;
    }
    g_term_active = 1;

    monitor_sample(&m);             /* First frame: Establish the difference baseline. */
    int rc = monitor_run(&m, &g_term);

    restore_terminal();
    monitor_shutdown(&m);
    return rc;
}