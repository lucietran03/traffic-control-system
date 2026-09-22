/*
    RMIT University Vietnam
    Course: EEET2588 Real-Time System Engineering
    Semester: 2026-2
    Author: Hoang Minh Thang
    ID: s3999925
    Week 7 - Lab Exercise 6 - Task 2B
    Due date: 28/08/2026
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/iofunc.h>
#include <unistd.h>

#define BUF_SIZE 100
#define INFO_FILE "/tmp/myServer.info"

typedef struct {
	struct _pulse hdr;  
	int ClientID;       
    int data;           
} my_data;

typedef struct {
	struct _pulse hdr;  
    char buf[BUF_SIZE]; 
} my_reply;

int server();

int main(int argc, char *argv[]) {
    // Task 2B: Server Code
    printf("Welcome to the Task 2B Server!\n");
	printf("Task 2B: Server running\n");
    int ret = server();
	printf("Main (Server) Terminated....\n");
	return ret;
}

int server() {
	int serverPID=0, chid=0; 	
	serverPID = getpid(); 		

	chid = ChannelCreate(_NTO_CHF_DISCONNECT);
	if (chid == -1) {
	    printf("\nFailed to create communication channel on server\n");
		return EXIT_FAILURE;
	}

	printf("Server Listening for Clients on:\n");
	printf("  --> Process ID   : %d \n", serverPID);
	printf("  --> Channel ID   : %d \n\n", chid);

    // Write the dynamic connection data to a file
    FILE *fp = fopen(INFO_FILE, "w");
    if (fp != NULL) {
        fprintf(fp, "%d\n%d\n", serverPID, chid);
        fclose(fp);
        printf("Successfully wrote PID and CHID to %s\n", INFO_FILE);
    } else {
        printf("Error: Cannot open file %s for writing\n", INFO_FILE);
        return EXIT_FAILURE;
    }

	my_data msg;
	int rcvid=0, msgnum=0;  	
	int Stay_alive=1, living=1;	

	my_reply replymsg; 			
	replymsg.hdr.type = 0x01;
	replymsg.hdr.subtype = 0x00;

	while (living) {
	   rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);

	   if (rcvid == -1) break;

	   if (rcvid == 0) { 
		   switch (msg.hdr.code) {
			   case _PULSE_CODE_DISCONNECT:
                   ConnectDetach(msg.hdr.scoid);
                   printf("\nServer received Detach pulse from ClientID:%d\n", msg.ClientID);
				   break;
		   }
		   continue;
	   }

	   if(rcvid > 0) {
		   msgnum++;
		   sprintf(replymsg.buf, "Message %d received", msgnum);
		   printf("Server received data packet with value of '%d' from client (ID:%d), ", msg.data, msg.ClientID);
		   fflush(stdout);
		   
		   printf("\n    -----> replying with: '%s'\n",replymsg.buf);
		   MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));
	   } 
	}
	ChannelDestroy(chid);
    remove(INFO_FILE); // Clean up the file on exit
	return EXIT_SUCCESS;
}