#ifndef C_LOGGER_H
#define C_LOGGER_H

// Initializes the local append-only log file for persisting timestamped events.
void c_logger_init(void);

// Writes a formatted, timestamped event to both the log file and stdout.
void c_logger_log(const char *fmt, ...);

#endif /* C_LOGGER_H */