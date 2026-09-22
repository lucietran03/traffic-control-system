/*
    RMIT University Vietnam
    Course: EEET2588 Real-Time System Engineering
    Semester: 2026-2
    Author: Hoang Minh Thang
    ID: s3999925
    Week 6 - Lab Exercise 5 - Task 3B
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

// Define the states of the traffic light system
enum states {
    State0_EWR_NSR, // State 0: East-West Red, North-South Red
    State1_EWR_NSR, // State 1: East-West Red, North-South Red
    State2_EWG_NSR, // State 2: East-West Green, North-South Red
    State3_EWY_NSR, // State 3: East-West Yellow, North-South Red
    State4_EWR_NSR, // State 4: East-West Red, North-South Red
    State5_EWR_NSG, // State 5: East-West Red, North-South Green
    State6_EWR_NSY  // State 6: East-West Red, North-South Yellow
};

typedef union {
	struct _pulse pulse;   // QNX pulse data, including code and sender details
} my_message_t;


int main(int argc, char *argv[]) {
	// Task 3B
    printf("Welcome to Task 3B - Timer-Driven Traffic Lights\n");

	enum states CurrentState = State0_EWR_NSR; // Start with State 0
    struct sigevent event;
    struct itimerspec itime_Short; // 1.0 second timer
    struct itimerspec itime_Long; // 2.0 second timer
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

    // Setup 1.0 second timer for Yellow/Red (itime_Short)
    itime_Short.it_value.tv_sec = 1;
    itime_Short.it_value.tv_nsec = 0;
    itime_Short.it_interval.tv_sec = 0;  // 0 makes it single shot
    itime_Short.it_interval.tv_nsec = 0;

    // Setup 2.0 second timer for Green (itime_Long)
    itime_Long.it_value.tv_sec = 2;
    itime_Long.it_value.tv_nsec = 0;
    itime_Long.it_interval.tv_sec = 0;  // 0 makes it single shot
    itime_Long.it_interval.tv_nsec = 0;

	// Start with the 1.0 second timer for State 0
	timer_settime(timer_id, 0, &itime_Short, NULL);

	int Runtimes = 30; // Number of timer expiries to process
	int counter = 0;   // Counts received timer pulses

    while (counter < Runtimes) {
        // Block until a message or pulse arrives. The received bytes are stored
		// in the union msg; for a pulse, access them through msg.pulse.
        rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);

        // MsgReceive() returns 0 for a pulse and a positive ID for a message.
        if (rcvid == 0 && msg.pulse.code == MY_PULSE_CODE) {
             switch (CurrentState) {
                case State0_EWR_NSR:
                    printf("State 0: EWR-NSR (Red-Red)\n");
                    CurrentState = State1_EWR_NSR;
                    timer_settime(timer_id, 0, &itime_Short, NULL);
                    break;
                case State1_EWR_NSR:
                    printf("State 1: EWR-NSR (Red-Red)\n");
                    CurrentState = State2_EWG_NSR;
                    timer_settime(timer_id, 0, &itime_Long, NULL); // Next is green
                    break;
                case State2_EWG_NSR:
                    printf("State 2: EWG-NSR (Green-Red) - Long Delay\n");
                    CurrentState = State3_EWY_NSR;
                    timer_settime(timer_id, 0, &itime_Short, NULL);
                    break;
                case State3_EWY_NSR:
                    printf("State 3: EWY-NSR (Yellow-Red)\n");
                    CurrentState = State4_EWR_NSR;
                    timer_settime(timer_id, 0, &itime_Short, NULL);
                    break;
                case State4_EWR_NSR:
                    printf("State 4: EWR-NSR (Red-Red)\n");
                    CurrentState = State5_EWR_NSG;
                    timer_settime(timer_id, 0, &itime_Long, NULL); // Next is green
                    break;
                case State5_EWR_NSG:
                    printf("State 5: EWR-NSG (Red-Green) - Long Delay\n");
                    CurrentState = State6_EWR_NSY;
                    timer_settime(timer_id, 0, &itime_Short, NULL);
                    break;
                case State6_EWR_NSY:
                    printf("State 6: EWR-NSY (Red-Yellow)\n");
                    CurrentState = State0_EWR_NSR;
                    timer_settime(timer_id, 0, &itime_Short, NULL);
                    break;
            }
            counter++;
            fflush(stdout); // Flush the output buffer to ensure timely display of messages   
        }
        // else other message
    }

	printf("\nSwitch statement got called %d times\n", counter);

	printf("All good. Main Terminated...\n\n");
	return EXIT_SUCCESS;
}