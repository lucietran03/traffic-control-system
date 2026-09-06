/*
    RMIT University Vietnam
    Course: EEET2588 Real-Time System Engineering
    Semester: 2026-2
    Author: Hoang Minh Thang
    ID: s3999925
    Week 6 - Lab Exercise 5 - Task 2D - File 1: Sensor Reader Process
    Due date: 21/08/2026
*/

#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

#define QUEUE_NAME "/sensor_queue"
#define MESSAGESIZE 2 // 2 bytes: one for character, one for \0

int main(void) {
    // Task 2D: Sensor Reader Process
    printf("Message Queue Sensor Reader Process started.\n");

    printf("Status: Initializing message queue writer...\n");
    

    mqd_t qd_writer;
    char msg[MESSAGESIZE];
    msg[1] = '\0'; // 2nd character is the \0 delimiter

    // Open the existing queue for writing only (located at /dev/mqueue/sensor_queue)
    qd_writer = mq_open(QUEUE_NAME, O_WRONLY);
    if (qd_writer == (mqd_t) - 1) {
        perror("\n[ERROR] Could not connect to queue. Please ensure the Main Traffic Light process is running first.\n");
        exit(1);
    }

    printf("Status: Successfully connected to '%s'.\n\n", QUEUE_NAME);
    printf("INSTRUCTIONS:\n");
    printf("Type a character to simulate a car arriving at the intersection:\n");
    printf("  'e' = Car waiting at East/West road\n");
    printf("  'n' = Car waiting at North/South road\n");
    printf("  'q' = Quit and cleanly shut down the sensor reader\n\n");

    while (1) {
        printf("-> Waiting for sensor trigger ('e' or 'n'): ");
        fflush(stdout); // ensure prompt is printed before waiting for input
        scanf(" %c", &msg[0]);

        if (msg[0] == 'q') {
            printf("Exiting loop...\n");
            break;
        } else if (msg[0] == 'e' || msg[0] == 'n') {
            if (mq_send(qd_writer, msg, MESSAGESIZE, 0) == -1) {
                perror("[ERROR] Failed to send message to state machine process");
            } else {
                printf("[SUCCESS]: Sensor trigger '%c' sent to state machine process.\n", msg[0]);
            }
        } else {
            printf("[INVALID] Unrecognized input. Please enter 'e' or 'n' or 'q'.\n");
        }
    }
    
    mq_close(qd_writer);
    printf("Sensor Reader Process cleanly shut down.\n");
    return 0;
}