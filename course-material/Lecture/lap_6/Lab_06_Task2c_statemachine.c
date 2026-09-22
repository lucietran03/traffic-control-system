/*
    RMIT University Vietnam
    Course: EEET2588 Real-Time System Engineering
    Semester: 2026-2
    Author: Hoang Minh Thang
    ID: s3999925
    Week 7 - Lab Exercise 6 - Task 2C
    Due date: 28/08/2026
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/iofunc.h>
#include <string.h>

#define BUF_SIZE 100
#define INFO_FILE "/tmp/myServer.info"

typedef struct {
    struct _pulse hdr;
    int ClientID;
    char data; // Changed to char to pass sensor trigger characters
} my_data;

typedef struct {
    struct _pulse hdr;
    char buf[BUF_SIZE];
} my_reply;

enum states {
    State0_EWR_NSR, // Initial state: East-West Red, North-South Red
    State1_EWR_NSR, // East-West Red, North-South Red
    State2_EWG_NSR, // East-West Green, North-South Red
    State3_EWY_NSR, // East-West Yellow, North-South Red
    State4_EWR_NSR, // East-West Red, North-South Red
    State5_EWR_NSG, // East-West Red, North-South Green 
    State6_EWR_NSY // East-West Red, North-South Yellow
};

struct sensor_data {
    char trigger_val; 
} shared_sensor = {' '};

pthread_mutex_t sensor_mutex = PTHREAD_MUTEX_INITIALIZER;

// Dedicated thread to handle receiving IPC messages
void *server_thread(void *arg) {
    int serverPID = getpid();
    int chid = ChannelCreate(_NTO_CHF_DISCONNECT);
    if (chid == -1) {
        printf("\n[ERROR] Failed to create communication channel.\n");
        pthread_exit((void*)EXIT_FAILURE);
    }

    FILE *fp = fopen(INFO_FILE, "w");
    if (fp != NULL) {
        fprintf(fp, "%d\n%d\n", serverPID, chid);
        fclose(fp);
    }

    my_data msg;
    my_reply replymsg;
    replymsg.hdr.type = 0x01;
    replymsg.hdr.subtype = 0x00;

    int rcvid;
    while (1) {
        rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);
        if (rcvid == -1) break;

        if (rcvid == 0) {
            if (msg.hdr.code == _PULSE_CODE_DISCONNECT) {
                ConnectDetach(msg.hdr.scoid);
            }
            continue;
        }

        if (rcvid > 0) {
            pthread_mutex_lock(&sensor_mutex);
            shared_sensor.trigger_val = msg.data; // Safely update shared struct
            pthread_mutex_unlock(&sensor_mutex);

            sprintf(replymsg.buf, "Sensor '%c' received", msg.data);
            MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));
        }
    }
    
    ChannelDestroy(chid);
    remove(INFO_FILE);
    return NULL;
}

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
            pthread_mutex_lock(&sensor_mutex);
            if (shared_sensor.trigger_val == 'n') { // Check for North/South sensor trigger
                shared_sensor.trigger_val = ' '; 
                pthread_mutex_unlock(&sensor_mutex);
                *CurState = State3_EWY_NSR;
            } else {
                pthread_mutex_unlock(&sensor_mutex);
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
            pthread_mutex_lock(&sensor_mutex);
            if (shared_sensor.trigger_val == 'e') { // Check for East-West sensor trigger
                shared_sensor.trigger_val = ' '; 
                pthread_mutex_unlock(&sensor_mutex);
                *CurState = State6_EWR_NSY;
            } else {
                pthread_mutex_unlock(&sensor_mutex);
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
    // Task 2C: Unnamed IPC Traffic Lights State Machine Process
    printf("Welcome to the Task 2C Unnamed IPC Traffic Lights State Machine Process!\n");
    printf("Unnamed IPC Traffic Lights State Machine Process started.\n");

    pthread_t srv_thread;
    pthread_create(&srv_thread, NULL, server_thread, NULL);
    sleep(1); // Allow thread time to create the info file

    printf("Status: IPC channel created and receiver thread active.\n");
    printf("Status: You may now launch the Sensor Reader Process.\n\n");
    printf("Initiating 7-State Traffic Light Sequence...\n\n");
    
    int Runtimes = 40; 
    int counter = 0;
    enum states CurrentState = State0_EWR_NSR; 

    while (counter < Runtimes) {
        SingleStep_TrafficLight_SM(&CurrentState); 
        counter++;
    }
    
    printf("\nTraffic light sequence completed (%d iterations).\n", Runtimes);
    pthread_cancel(srv_thread);
    pthread_join(srv_thread, NULL);
    pthread_mutex_destroy(&sensor_mutex);
    printf("Main Controller Process Terminated.\n");
    return 0;
}