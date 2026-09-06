/*
    RMIT University Vietnam
    Course: EEET2588 Real-Time System Engineering
    Semester: 2026-2
    Author: Hoang Minh Thang
    ID: s3999925
    Week 6 - Lab Exercise 5 - Task 2D - Main Traffic Light State Machine Process
    Due date: 21/08/2026
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

#define QUEUE_NAME "/sensor_queue"
#define MESSAGESIZE 2 // 2 bytes: one for character, one for \0


// Define the 7 states representing the traffic light sequence
enum states {
    State0_EWR_NSR, // Initial state: East-West Red, North-South Red
    State1_EWR_NSR, // State 1: East-West Red, North-South Red
    State2_EWG_NSR, // State 2: East-West Green, North-South Red
    State3_EWY_NSR, // State 3: East-West Yellow, North-South Red
    State4_EWR_NSR, // State 4: East-West Red, North-South Red
    State5_EWR_NSG, // State 5: East-West Red, North-South Green
    State6_EWR_NSY // State 6: East-West Red, North-South Yellow
};

// Shared struct to hold sensor data as requested
// 'n' for North-South sensor, 'e' for East-West sensor, ' ' for no trigger
struct sensor_data {
    char trigger_val; 
} shared_sensor = {' '};

// Mutex to protect the shared sensor data
pthread_mutex_t sensor_mutex = PTHREAD_MUTEX_INITIALIZER;

// Dedicated thread function to handle receiving mqueue messages
void *mqueue_receiver_thread(void *arg) {
    mqd_t qd_reader;
    struct mq_attr attr;
    char recv_msg[MESSAGESIZE];

    // Configure message queue attributes
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MESSAGESIZE; // 2 bytes: one for character, one for \0
    attr.mq_curmsgs = 0;

    // Open the queue as read-only and create it if it doesn't exist (located at /dev/mqueue/sensor_queue)
    qd_reader = mq_open(QUEUE_NAME, O_RDONLY | O_CREAT, 0664, &attr);
    if (qd_reader == (mqd_t) - 1) {
        perror("State machine process: mq_open error");
        pthread_exit(NULL);
    }

    while (1) {
        // Block until a 2-byte message is received
        if (mq_receive(qd_reader, recv_msg, MESSAGESIZE, NULL) > 0) {
            pthread_mutex_lock(&sensor_mutex);
            shared_sensor.trigger_val = recv_msg[0]; // Safely update shared struct 
            pthread_mutex_unlock(&sensor_mutex);
        }
    }
    
    mq_close(qd_reader);
    return NULL;
}

// State machine function passing the current state by address
void SingleStep_TrafficLight_SM(enum states *CurState) {
    switch (*CurState) {
        case State0_EWR_NSR:
            printf("State 0: EWR-NSR (Red-Red)\n");
            sleep(1);
            *CurState = State1_EWR_NSR;
            break;
            
        case State1_EWR_NSR:
            printf("State 1: EWR-NSR (Red-Red)\n");
            sleep(1);
            *CurState = State2_EWG_NSR;
            break;
            
        case State2_EWG_NSR:
            printf("State 2: EWG-NSR (Green-Red)\n");

            pthread_mutex_lock(&sensor_mutex); // Lock mutex
            if (shared_sensor.trigger_val == 'n') {
                shared_sensor.trigger_val = ' '; // Reset the sensor
                pthread_mutex_unlock(&sensor_mutex); // Unlock mutex
                *CurState = State3_EWY_NSR;
            } else {
                pthread_mutex_unlock(&sensor_mutex); // Unlock mutex
                sleep(1);
                *CurState = State2_EWG_NSR;
            }
            break;
            
        case State3_EWY_NSR:
            printf("State 3: EWY-NSR (Yellow-Red)\n");
            sleep(1);
            *CurState = State4_EWR_NSR;
            break;
            
        case State4_EWR_NSR:
            printf("State 4: EWR-NSR (Red-Red)\n");
            sleep(1);
            *CurState = State5_EWR_NSG;
            break;
            
        case State5_EWR_NSG:
            printf("State 5: EWR-NSG (Red-Green)\n");

            pthread_mutex_lock(&sensor_mutex); // Lock mutex
            if (shared_sensor.trigger_val == 'e') {
                shared_sensor.trigger_val = ' '; // Reset the sensor
                pthread_mutex_unlock(&sensor_mutex); // Unlock mutex
                *CurState = State6_EWR_NSY;
            } else {
                pthread_mutex_unlock(&sensor_mutex); // Unlock mutex
                sleep(1);
                *CurState = State5_EWR_NSG;
            }
            break;
            
        case State6_EWR_NSY:
            printf("State 6: EWR-NSY (Red-Yellow)\n");
            sleep(1);
            *CurState = State0_EWR_NSR;
            break;
    }
}

int main(int argc, char *argv[]) {
    // Task 2D: Main Traffic Light State Machine Process
    printf("Message Queue Traffic Lights State Machine Process started.\n");
    
     printf("Status: Creating message queue '%s'...\n", QUEUE_NAME);

    // Create thread to wait for messages
    pthread_t mq_thread;
    pthread_create(&mq_thread, NULL, mqueue_receiver_thread, NULL);

    printf("Status: Queue created and receiver thread active.\n");
    printf("Status: You may now launch the Sensor Reader Process in a separate terminal.\n\n");
    printf("Initiating 7-State Traffic Light Sequence...\n\n");
    
    int Runtimes = 40; 
    int counter = 0;
    enum states CurrentState = State0_EWR_NSR; // Declared in main per instructions

    while (counter < Runtimes) {
        SingleStep_TrafficLight_SM(&CurrentState); 
        counter++;
    }
    
    // Cleanup resources
    printf("\nTraffic light sequence completed (%d iterations).\n", Runtimes);
    printf("Status: Cleaning up IPC resources...\n");
    pthread_cancel(mq_thread);
    pthread_join(mq_thread, NULL);
    mq_unlink(QUEUE_NAME); // Remove the queue from the system
    printf("Status: Message queue unlinked successfully.\n");
    printf("Main Controller Process Terminated.\n");

    pthread_mutex_destroy(&sensor_mutex); // Destroy the mutex
    return 0;
}
