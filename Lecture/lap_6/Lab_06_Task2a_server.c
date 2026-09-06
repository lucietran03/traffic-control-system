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
#include <unistd.h>

#define BUF_SIZE 100

typedef struct {
	struct _pulse hdr;  // Our real data comes after this header
	int ClientID;       // our data (unique id from client)
    int data;           // our data
} my_data;

typedef struct {
	struct _pulse hdr;  // Our real data comes after this header
    char buf[BUF_SIZE]; // Message we send back to clients
} my_reply;

int server();

int main(int argc, char *argv[]) {
    // Task 2A: Server Code
    printf("Welcome to the Task 2A Server!\n");
	printf("Task 2A: Server running\n");
    int ret = server();
	printf("Main (Server) Terminated....\n");
	return ret;
}

int server() {
	int serverPID=0, chid=0; 	
	serverPID = getpid(); 		// get server process ID

	// Create Channel
	chid = ChannelCreate(_NTO_CHF_DISCONNECT);
	if (chid == -1) {
	    printf("\nFailed to create communication channel on server\n");
		return EXIT_FAILURE;
	}

	printf("Server Listening for Clients on:\n");
	printf("  --> Process ID   : %d \n", serverPID);
	printf("  --> Channel ID   : %d \n\n", chid);

	my_data msg;
	int rcvid=0, msgnum=0;  	
	int Stay_alive=1, living=1;	// Stay alive to test multiple connections

	my_reply replymsg; 			
	replymsg.hdr.type = 0x01;
	replymsg.hdr.subtype = 0x00;

	while (living) {
	   rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);

	   if (rcvid == -1) { 
		   printf("\nFailed to MsgReceive\n");
		   break;
	   }

	   if (rcvid == 0) { 
		   switch (msg.hdr.code) {
			   case _PULSE_CODE_DISCONNECT:
				   if( Stay_alive == 0) {
					   ConnectDetach(msg.hdr.scoid);
					   printf("\nServer was told to Detach from ClientID:%d ...\n", msg.ClientID);
					   living = 0; 
					   continue;
				   } else {
                       ConnectDetach(msg.hdr.scoid);
					   printf("\nServer received Detach pulse from ClientID:%d but rejected termination ...\n", msg.ClientID);
				   }
				   break;
			   default:
				   printf("\nServer got some other pulse after %d, msgnum\n", msgnum);
				   break;
		   }
		   continue;
	   }

	   if(rcvid > 0) {
		   msgnum++;
		   if (msg.hdr.type == _IO_CONNECT ) {
			   MsgReply( rcvid, EOK, NULL, 0 );
			   printf("\n gns service is running....");
			   continue;	
		   }
		   if (msg.hdr.type > _IO_BASE && msg.hdr.type <= _IO_MAX ) {
			   MsgError( rcvid, ENOSYS );
			   printf("\n Server received and IO message and rejected it....");
			   continue;	
		   }

		   sprintf(replymsg.buf, "Message %d received", msgnum);
		   printf("Server received data packet with value of '%d' from client (ID:%d), ", msg.data, msg.ClientID);
		   fflush(stdout);
		   
		   printf("\n    -----> replying with: '%s'\n",replymsg.buf);
		   MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));
	   } else {
		   printf("\nERROR: Server received something, but could not handle it correctly\n");
	   }
	}

	printf("\nServer received Destroy command\n");
	ChannelDestroy(chid);
	return EXIT_SUCCESS;
}