/*
    RMIT University Vietnam
    Course: EEET2588 Real-Time System Engineering
    Semester: 2026-2
    Author: Hoang Minh Thang
    ID: s3999925
    Week 6 - Lab Exercise 5 - Task 1A
    Due date: 21/08/2026
*/

#include <stdio.h>
#include <stdbool.h>

enum states { State0, State1, State2, State3 };
enum states CurState = State0;
bool Done = false;

// Helper function to simulate the Test conditions via user input
int prompt_test(const char* test_name) {
    int input;
    int status;
    
    while (1) {
        printf("Condition %s (Enter non-zero for True, 0 for False): ", test_name);
        fflush(stdout); // Flushes the buffer so the prompt prints BEFORE scanf blocks
        
        status = scanf("%d", &input);
        
        if (status == 1) {
            // Successfully read an integer
            break; 
        } else {
            // Clear the invalid characters from the input buffer
            while (getchar() != '\n'); 
            printf("Invalid input. Please enter a valid number.\n\n");
        }
    }

    return input;
}

int main(void) {
    // Task 1A
    printf("Welcome to Task 1A - State Machine Example\n");
    
    while (!Done) {
        switch (CurState) {
            case State0:
                printf("\n[Current State: State0] Initializing...\n");
                CurState = State1; 
                break;
                
            case State1:
                printf("\n[Current State: State1] Doing something 1...\n");
                if (prompt_test("TEST1")) {
                    CurState = State3; // User choose true
                } else {
                    CurState = State2; // User choose false
                }
                break;
                
            case State2:
                printf("\n[Current State: State2] Doing something 2...\n");
                if (prompt_test("TEST2")) {
                    CurState = State3; // User choose true
                } else {
                    CurState = State0; // User choose false
                }
                break;
                
            case State3:
                printf("\n[Current State: State3] Doing something 3...\n");
                if (!prompt_test("TEST3")) {
                    CurState = State0; // User choose false
                } else {
                    // Stays in State 3 if user chose true
                    printf("Remaining in State 3.\n"); 
                }
                break;
        }
    }
    return 0;
}