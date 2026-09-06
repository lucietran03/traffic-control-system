/*
    RMIT University Vietnam
    Course: EEET2588 Real-Time System Engineering
    Semester: 2026-2
    Author: Hoang Minh Thang
    ID: s3999925
    Week 6 - Lab Exercise 5 - Task 2B
    Due date: 21/08/2026
*/

#include <stdio.h>
#include <unistd.h>

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

// Helper function to simulate reading a hardware sensor via keyboard
char prompt_sensor() {
    char input;
    printf("-> Sensor input ('e' for car waiting at East/West road, 'n' for car waiting at North/South road): ");
    fflush(stdout); // Flush buffer so prompt appears BEFORE scanf blocks
    
    scanf(" %c", &input);
    
    while (getchar() != '\n'); // Clear any extra characters from the buffer
    return input;
}

// State machine function passing the current state by address
void SingleStep_TrafficLight_SM(enum states *CurState) {
    char sensor_val;
    
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
            sensor_val = prompt_sensor(); // Simulate sensor input
            if (sensor_val == 'n') {
                *CurState = State3_EWY_NSR; // North/South car waiting, change lights
            } else {
                *CurState = State2_EWG_NSR; // No car, remain green
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
            sensor_val = prompt_sensor();  // Simulate sensor input
            if (sensor_val == 'e') { 
                *CurState = State6_EWR_NSY; // East/West car waiting, change lights
            } else {
                *CurState = State5_EWR_NSG; // No car, remain green
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
    // Task 2B
    printf("Welcome to Task 2B - Sensor Driven Traffic Lights State Machine\n");
    
    int Runtimes = 30; 
    int counter = 0;
    enum states CurrentState = State0_EWR_NSR;

    while (counter < Runtimes) {
        SingleStep_TrafficLight_SM(&CurrentState); 
        fflush(stdout); // Ensure output is printed immediately
        counter++;
    }
    
    printf("\nMain Terminating....\n");
    return 0;
}