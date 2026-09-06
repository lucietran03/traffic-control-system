/*
    RMIT University Vietnam
    Course: EEET2588 Real-Time System Engineering
    Semester: 2026-2
    Author: Hoang Minh Thang
    ID: s3999925
    Week 6 - Lab Exercise 5 - Task 2A
    Due date: 21/08/2026
*/

#include <stdio.h>
#include <unistd.h>

// Define the 7 states representing the traffic light sequence 
// Red/Yellow: 1 second delay, Green: 2 seconds delay
enum states {
    State0_EWR_NSR, // East-West Red, North-South Red
    State1_EWR_NSR, // East-West Red, North-South Red
    State2_EWG_NSR, // East-West Green, North-South Red
    State3_EWY_NSR, // East-West Yellow, North-South Red
    State4_EWR_NSR, // East-West Red, North-South Red
    State5_EWR_NSG, // East-West Red, North-South Green
    State6_EWR_NSY  // East-West Red, North-South Yellow
};

// State machine function passing the current state by address
void SingleStep_TrafficLight_SM(enum states *CurState) {
    switch (*CurState) {
        case State0_EWR_NSR:
            printf("State 0: EWR-NSR (East-West Red, North-South Red)\n");
            sleep(1); // Short delay (1 second)
            *CurState = State1_EWR_NSR;
            break;
            
        case State1_EWR_NSR:
            printf("State 1: EWR-NSR (East-West Red, North-South Red)\n");
            sleep(1); // Short delay (1 second)
            *CurState = State2_EWG_NSR;
            break;
            
        case State2_EWG_NSR:
            printf("State 2: EWG-NSR (East-West Green, North-South Red) - Long Delay\n");
            sleep(2); // Long delay (2 seconds) for Green lights
            *CurState = State3_EWY_NSR;
            break;
            
        case State3_EWY_NSR:
            printf("State 3: EWY-NSR (East-West Yellow, North-South Red)\n");
            sleep(1); // Short delay (1 second)
            *CurState = State4_EWR_NSR;
            break;
            
        case State4_EWR_NSR:
            printf("State 4: EWR-NSR (East-West Red, North-South Red)\n");
            sleep(1); // Short delay (1 second)
            *CurState = State5_EWR_NSG;
            break;
            
        case State5_EWR_NSG:
            printf("State 5: EWR-NSG (East-West Red, North-South Green) - Long Delay\n");
            sleep(2); // Long delay (2 seconds) for Green lights
            *CurState = State6_EWR_NSY;
            break;
            
        case State6_EWR_NSY:
            printf("State 6: EWR-NSY (East-West Red, North-South Yellow)\n");
            sleep(1); // Short delay (1 second)
            *CurState = State0_EWR_NSR;
            break;
    }
}

int main(int argc, char *argv[]) {
    // Task 2A
    printf("Welcome to Task 2A -Fixed Sequence Traffic Lights State Machine\n");
    
    int Runtimes = 30; 
    int counter = 0;
    
    // Initialize the starting state
    enum states CurrentState = State0_EWR_NSR;

    // Outer loop to drive the state machine
    while (counter < Runtimes) {
        // Pass the address of CurrentState to allow the function to update it
        SingleStep_TrafficLight_SM(&CurrentState); 
        fflush(stdout); // Ensure output is printed immediately
        counter++;
    }
    
    printf("\nMain Terminating....\n");
    return 0;
}