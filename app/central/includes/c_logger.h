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

#ifndef C_LOGGER_H
#define C_LOGGER_H

// Initializes the local append-only log file for persisting timestamped events.
void c_logger_init(void);

// Writes a formatted, timestamped event to both the log file and stdout.
void c_logger_log(const char *fmt, ...);

#endif /* C_LOGGER_H */