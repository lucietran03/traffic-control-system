/*
    RMIT University Vietnam
    Course: EEET2588 Real-Time System Engineering
    Semester: 2026-2
    Author: Hoang Minh Thang
    ID: s3999925
    Week 6 - Lab Exercise 5 - Task 3A
    Due date: 21/08/2026
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/netmgr.h>
#include <sys/neutrino.h>
#include <errno.h>

#define MY_PULSE_CODE   _PULSE_CODE_MINAVAIL

enum states { State0, State1, State2};

typedef union {
	struct _pulse pulse;   // QNX pulse data, including code and sender details
} my_message_t;


int main(int argc, char *argv[]) {
	// Task 3A
    printf("Welcome to Task 3A - POSIX Periodic Timers\n");

	enum states CurrentState = State0;
    struct sigevent event;
    struct itimerspec itime_A; // 1.5 second timer
    struct itimerspec itime_B; // 5.0 second timer
    timer_t timer_id;
    int chid, rcvid;
    my_message_t msg;

	// Create channel
    chid = ChannelCreate(0);
    event.sigev_notify = SIGEV_PULSE;
    event.sigev_coid = ConnectAttach(ND_LOCAL_NODE, 0, chid, _NTO_SIDE_CHANNEL, 0);

	// Use the current thread priority for the timer pulse.
	struct sched_param th_param;
	pthread_getschedparam(pthread_self(), NULL, &th_param);
	event.sigev_priority = th_param.sched_curpriority;
	event.sigev_code = MY_PULSE_CODE;

	// Create the timer using CLOCK_REALTIME and bind it to event.
	// timer_id receives the handle needed to start or modify the timer.
	if (timer_create(CLOCK_REALTIME, &event, &timer_id) == -1) {
	   perror ("Couldn't create a timer");
	   exit (EXIT_FAILURE);
	}

    // Setup 1.5 second timer (itime_A)
    itime_A.it_value.tv_sec = 1;
    itime_A.it_value.tv_nsec = 500000000;
    itime_A.it_interval.tv_sec = 0;  // 0 makes it single shot
    itime_A.it_interval.tv_nsec = 0;

    // Setup 5.0 second timer (itime_B)
    itime_B.it_value.tv_sec = 5;
    itime_B.it_value.tv_nsec = 0;
    itime_B.it_interval.tv_sec = 0;  // 0 makes it single shot
    itime_B.it_interval.tv_nsec = 0;

	// Start with the 1.5 second timer for State 0
	timer_settime(timer_id, 0, &itime_A, NULL);

	int Runtimes = 10; // Number of timer expiries to process
	int counter = 0;   // Counts received timer pulses

	for (counter = 0; counter < Runtimes; counter++) {
		// Block until a message or pulse arrives. The received bytes are stored
		// in the union msg; for a pulse, access them through msg.pulse.
	   rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);

	   // MsgReceive() returns 0 for a pulse and a positive ID for a message.
	   if (rcvid == 0 && msg.pulse.code == MY_PULSE_CODE) {
            printf("In state = %d\n", CurrentState);
            switch (CurrentState) {
                case State0:
                    CurrentState = State1;
                    // Next state is State 1, so set timer to 5 seconds
                    timer_settime(timer_id, 0, &itime_B, NULL);
                    break;
                case State1:
                    CurrentState = State2;
                    // Next state is State 2, so set timer to 1.5 seconds
                    timer_settime(timer_id, 0, &itime_A, NULL);
                    break;
                case State2:
                    CurrentState = State0;
                    // Next state is State 0, so set timer to 1.5 seconds
                    timer_settime(timer_id, 0, &itime_A, NULL);
                    break;
            }
            fflush(stdout); // Flush the output buffer to ensure timely display of messages
        }
	   // else other messages ...
	}

	printf("\nSwitch statement got called %d times\n", counter);

	printf("All good. Main Terminated...\n\n");
	return EXIT_SUCCESS;
}

