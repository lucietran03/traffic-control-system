#ifndef C_LOGGER_H
#define C_LOGGER_H

/* Opens the log file (append mode) - call once from c_main.c's main()
 * before the server loop starts. Uses a plain local text file
 * ("central_log.txt" in the working directory) rather than QNX's /fs,
 * since this needs to run identically on the dev host and a real QNX
 * target for this PoC - swap for /fs/central_log.txt on real hardware. */
void c_logger_init(void);

/* Writes one timestamped line (wall-clock via <time.h>, for a human
 * reading the log - unrelated to the IPC monotonic-clock gap elsewhere
 * in this codebase) to both the log file and stdout. printf-style
 * variadic, no trailing newline needed in fmt (added automatically). */
void c_logger_log(const char *fmt, ...);

#endif /* C_LOGGER_H */
