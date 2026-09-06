/*
    RMIT University Vietnam
    Course: EEET2588 Real-Time System Engineering
    Semester: 2026-2
    Author: Hoang Minh Thang
    ID: s3999925
    Week 7 - Lab Exercise 6 - Task 2A
    Due date: 28/08/2026
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/iofunc.h>
#include <sys/netmgr.h>
#include <unistd.h>

#define BUF_SIZE 100

typedef struct {
	struct _pulse hdr;  
	int ClientID;       
    int data;           
} my_data;

typedef struct {
	struct _pulse hdr;  
    char buf[BUF_SIZE]; 
} my_reply;

int client(int serverPID, int serverCHID);

int main(int argc, char *argv[]) {
    // Task 2A: Client Code
    printf("Welcome to the Task 2A Client!\n");
	printf("Task 2A: Client running\n");

	int serverPID  = 868381; // Update this PID manually after starting the server
	int	serverCHID = 1;			

	int ret = client(serverPID, serverCHID);

	printf("Main (client) Terminated....\n");
	return ret;
}

int client(int serverPID,  int serverChID) {
    my_data msg;
    my_reply reply;

    msg.ClientID = 500;
    int server_coid;
    int index = 0;

	printf("   --> Trying to connect (server) process which has a PID: %d\n", serverPID);
	printf("   --> on channel: %d\n\n", serverChID);

    server_coid = ConnectAttach(ND_LOCAL_NODE, serverPID, serverChID, _NTO_SIDE_CHANNEL, 0);
	if (server_coid == -1) {
        printf("\n    ERROR, could not connect to server!\n\n");
        return EXIT_FAILURE;
	}

    printf("Connection established to process with PID:%d, Ch:%d\n", serverPID, serverChID);

    msg.hdr.type = 0x00;
    msg.hdr.subtype = 0x00;

    for (index=0; index < 5; index++) {
    	msg.data=10+index;
        printf("Client (ID:%d), sending data packet with the integer value: %d \n", msg.ClientID, msg.data);
        fflush(stdout);

        if (MsgSend(server_coid, &msg, sizeof(msg), &reply, sizeof(reply)) == -1) {
            printf(" Error data '%d' NOT sent to server\n", msg.data);
            break;
        } else { 
            printf("   -->Reply is: '%s'\n", reply.buf);
        }
    }

    printf("\n Sending message to server to tell it to close the connection\n");
    ConnectDetach(server_coid);
    return EXIT_SUCCESS;
}