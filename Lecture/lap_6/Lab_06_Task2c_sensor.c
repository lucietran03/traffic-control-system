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
#include <sys/iofunc.h>
#include <sys/netmgr.h>
#include <unistd.h>
#include <pthread.h>

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

struct conn_info {
    int pid;
    int chid;
};

// Thread to handle keyboard reading and sending messages
void *client_thread(void *arg) {
    struct conn_info *info = (struct conn_info *)arg;
    int server_coid = ConnectAttach(ND_LOCAL_NODE, info->pid, info->chid, _NTO_SIDE_CHANNEL, 0);
    if (server_coid == -1) {
        printf("\n[ERROR] Could not connect. Ensure State Machine is running.\n");
        pthread_exit((void*)EXIT_FAILURE);
    }

    my_data msg;
    my_reply reply;
    msg.ClientID = 500;
    msg.hdr.type = 0x00;
    msg.hdr.subtype = 0x00;

    printf("INSTRUCTIONS:\n");
    printf("  'e' = Car waiting at East/West road\n");
    printf("  'n' = Car waiting at North/South road\n");
    printf("  'q' = Quit sensor reader\n\n");

    char input;
    while (1) {
        printf("-> Waiting for sensor trigger ('e' or 'n'): ");
        fflush(stdout); 
        scanf(" %c", &input);

        if (input == 'q') {
            printf("Exiting loop...\n");
            break;
        } else if (input == 'e' || input == 'n') {
            msg.data = input;
            if (MsgSend(server_coid, &msg, sizeof(msg), &reply, sizeof(reply)) == -1) {
                printf("[ERROR] Failed to send message.\n");
                break;
            } else {
                printf("[SUCCESS]: Sensor trigger '%c' sent to state machine process.\n", input);
            }
        } else {
            printf("[INVALID] Unrecognized input.\n");
        }
    }
    
    ConnectDetach(server_coid);
    return NULL;
}

int main(void) {
    // Task 2C: Unnamed IPC Sensor Reader Process
    printf("Welcome to the Task 2C Unnamed IPC Sensor Reader Process!\n");
    printf("Unnamed IPC Sensor Reader Process started.\n");

    int serverPID = 0, serverCHID = 0;
    FILE *fp = fopen(INFO_FILE, "r");
    if (fp != NULL) {
        fscanf(fp, "%d\n%d", &serverPID, &serverCHID);
        fclose(fp);
        printf("Successfully read PID (%d) and CHID (%d)\n", serverPID, serverCHID);
    } else {
        printf("[ERROR] Could not open %s. Is the server running?\n", INFO_FILE);
        return EXIT_FAILURE;
    }

    struct conn_info info = {serverPID, serverCHID};
    pthread_t cli_thread;
    pthread_create(&cli_thread, NULL, client_thread, (void*)&info);
    
    pthread_join(cli_thread, NULL);
    printf("Sensor Reader Process cleanly shut down.\n");
    return 0;
}