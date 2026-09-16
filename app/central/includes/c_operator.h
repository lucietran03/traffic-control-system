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

#ifndef C_OPERATOR_H
#define C_OPERATOR_H

#include <pthread.h>

#include "sys_types.h"
#include "qnet_utils.h"
#include "c_mode_eng.h"

// Operator console interface to process blocking stdin commands on a dedicated thread.

// Thread arguments and synchronization locks for the operator console loop.
typedef struct {
    ipc_client_queue_t *client_queue;
    c_mode_eng_t        *mode_eng;
    pthread_mutex_t      *mode_eng_lock;
    pthread_mutex_t      *console_io_lock;
} c_operator_args_t;

// Main blocking loop to read and dispatch manual operator commands.
void *c_operator_reader_thread(void *arg);

#endif /* C_OPERATOR_H */