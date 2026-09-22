/*
    RMIT University Vietnam
    Course: EEET2588 Real-Time System Engineering
    Semester: 2026-2
    Author: Hoang Minh Thang
    ID: s3999925
    Week 6 - Lab Exercise 5 - Task 2C
    Due date: 21/08/2026
*/

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

// Define the 7 states representing the traffic light sequence
enum states {
    State0_EWR_NSR, // East-West Red, North-South Red
    State1_EWR_NSR, // East-West Red, North-South Red
    State2_EWG_NSR, // East-West Green, North-South Red
    State3_EWY_NSR, // East-West Yellow, North-South Red
    State4_EWR_NSR, // East-West Red, North-South Red
    State5_EWR_NSG, // East-West Red, North-South Green
    State6_EWR_NSY  // East-West Red, North-South Yellow
};

// Global shared variable to hold sensor data
volatile char shared_sensor_val = ' ';

// Dedicated thread function to handle blocking keyboard input
// 'n' for waiting car at North/South road, 'e' for waiting car at East/West road
void *keyboard_reader_thread(void *arg) {
    while (1) {
        char input;
        scanf(" %c", &input);
        shared_sensor_val = input; // Safely update the shared variable
    }
    return NULL;
}

// State machine function passing the current state by address
void SingleStep_TrafficLight_SM(enum states *CurState) {
    switch (*CurState) {
        case State0_EWR_NSR:
            printf("State 0: EWR-NSR (East-West Red, North-South Red)\n");
            sleep(1);
            *CurState = State1_EWR_NSR;
            break;
            
        case State1_EWR_NSR:
            printf("State 1: EWR-NSR (East-West Red, North-South Red)\n");
            sleep(1);
            *CurState = State2_EWG_NSR;
            break;
            
        case State2_EWG_NSR:
            printf("State 2: EWG-NSR (East-West Green, North-South Red)\n");
            // Check the shared variable instead of prompting
            if (shared_sensor_val == 'n') {
                shared_sensor_val = ' '; // Reset the sensor
                *CurState = State3_EWY_NSR;
            } else {
                sleep(1); // Prevent console flooding while green
                *CurState = State2_EWG_NSR;
            }
            break;
            
        case State3_EWY_NSR:
            printf("State 3: EWY-NSR (East-West Yellow, North-South Red)\n");
            sleep(1);
            *CurState = State4_EWR_NSR;
            break;
            
        case State4_EWR_NSR:
            printf("State 4: EWR-NSR (East-West Red, North-South Red)\n");
            sleep(1);
            *CurState = State5_EWR_NSG;
            break;
            
        case State5_EWR_NSG:
            printf("State 5: EWR-NSG (East-West Red, North-South Green)\n");
            // Check the shared variable instead of prompting
            if (shared_sensor_val == 'e') {
                shared_sensor_val = ' '; // Reset the sensor
                *CurState = State6_EWR_NSY;
            } else {
                sleep(1); // Prevent console flooding while green
                *CurState = State5_EWR_NSG;
            }
            break;
            
        case State6_EWR_NSY:
            printf("State 6: EWR-NSY (East-West Red, North-South Yellow)\n");
            sleep(1);
            *CurState = State0_EWR_NSR;
            break;
    }
}

int main(int argc, char *argv[]) {
    // Task 2C
    printf("Welcome to Task 2C - Shared Variable Sensor Thread State Machine\n");

    printf("\nInstructions: Press 'e' for car waiting at East/West road, 'n' for car waiting at North/South road\n\n");
 
    // Create the separate thread to read the keyboard
    pthread_t kb_thread;
    pthread_create(&kb_thread, NULL, keyboard_reader_thread, NULL);
    
    int Runtimes = 30; 
    int counter = 0;
    enum states CurrentState = State0_EWR_NSR;

    // The main thread continues to run the state machine independently
    while (counter < Runtimes) {
        SingleStep_TrafficLight_SM(&CurrentState); 
        fflush(stdout); // Ensure output is printed immediately
        counter++;
    }
    
    // Cancel the background thread before exiting
    pthread_cancel(kb_thread);
    printf("\nMain Terminating....\n");
    return 0;
}