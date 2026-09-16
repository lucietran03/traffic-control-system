/*
    RMIT University Vietnam
    Course: EEET2588 Real-Time System Engineering
    Semester: 2026-2
    Author: Team QNX
    Member: Tran Dong Nghi - s3914633
			Le Hung - s4061665
			Hoang Minh Thang - s3999925
    Assessment: 2 - Project Implementation 
    Due date: 18/09/2026
*/

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "c_logger.h"

// Module-private file handle; falls back to stdout if initialization fails.
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