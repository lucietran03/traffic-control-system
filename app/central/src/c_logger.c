#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "c_logger.h"

/*
 * Static/module-private log file handle. NULL means either "not
 * initialised yet" or "fopen() failed" - c_logger_log() falls back to
 * stdout-only output in both cases (see doc comment in c_logger.h).
 */
static FILE *g_log_file = NULL;

void c_logger_init(void)
{
    g_log_file = fopen("central_log.txt", "a");
    if (g_log_file == NULL) {
        fprintf(stdout, "C1: c_logger_init: could not open central_log.txt - logging to stdout only\n");
    }
}

void c_logger_log(const char *fmt, ...)
{
    time_t     now = time(NULL);
    struct tm *tm_info;
    char       timestamp[32];
    va_list    args;

    tm_info = localtime(&now);
    if (tm_info != NULL) {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    } else {
        timestamp[0] = '\0';
    }

    fprintf(stdout, "[%s] ", timestamp);
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
    fprintf(stdout, "\n");

    if (g_log_file != NULL) {
        fprintf(g_log_file, "[%s] ", timestamp);
        va_start(args, fmt);
        vfprintf(g_log_file, fmt, args);
        va_end(args);
        fprintf(g_log_file, "\n");
        fflush(g_log_file);
    }
}
