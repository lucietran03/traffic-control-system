/*
    RMIT University Vietnam
    Course: EEET2588 Real-Time System Engineering
    Semester: 2026-2
    Author: Hoang Minh Thang
    ID: s3999925
    Week 7 - Lab Exercise 6 - Task 1A
    Due date: 28/08/2026
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/dispatch.h>

// Define where the "local" attach point is located. It will be located at <hostname>/dev/name/local/thang"
#define ATTACH_POINT "thang"

#define BUF_SIZE 100

typedef union {	  			// This replaced the standard:  union sigval
	union{
		_Uint32t sival_int;
		void *sival_ptr;	// This has a different size in 32-bit and 64-bit systems
	};
	_Uint32t dummy[4]; 		// Hence, we need this dummy variable to create space
}_mysigval;

typedef struct _Mypulse {   // This replaced the standard:  typedef struct _pulse msg_header_t;
    _Uint16t type;
    _Uint16t subtype;
    _Int8t code;
    _Uint8t zero[3];         // Same padding that is used in standard _pulse struct
    _mysigval value;
    _Uint8t zero2[2];		// Extra padding to ensure alignment access.
    _Int32t scoid;
} msg_header_t;

typedef struct {
    msg_header_t hdr;  // Custom header
    int ClientID;      // our data (unique id from client)
    int data;          // our data 
} my_data;

typedef struct {
    msg_header_t hdr;   // Custom header
    char buf[BUF_SIZE]; // Message to send back to send back to other thread
} my_reply;

// prototypes
int server();

int main(int argc, char *argv[]) {
    // Task 1A: Server code
    printf("Welcome to the Task 1A Server!\n");
    printf("Server running\n");

    int ret = 0;
    ret = server();

	printf("Main (Server) Terminated....\n");
	return ret;
}


/*** Server code ***/
int server() {
	printf("_mysigval size= %d\n", sizeof(_mysigval));
	printf("header size= %d\n", sizeof(msg_header_t));
	printf("my_data size= %d\n", sizeof(my_data));
	printf("my_reply size= %d\n", sizeof(my_reply));

    name_attach_t *attach;
    my_data msg;
    my_reply replymsg; // replymsg structure for sending back to client

    replymsg.hdr.type = 0x01;       // some number to help client process reply msg
    replymsg.hdr.subtype = 0x00;    // some number to help client process reply msg

    // Create a global name (/dev/name/local/...)
    if ((attach = name_attach(NULL, ATTACH_POINT, 0)) == NULL) {
        printf("\nFailed to name_attach on ATTACH_POINT: %s \n", ATTACH_POINT);
        return EXIT_FAILURE;
    }

    printf("Server Listening for Clients on ATTACH_POINT: %s \n", ATTACH_POINT);

   	/*
	 *  Server Loop
	 */
    int rcvid=0, msgnum=0;  		// no message received yet
    int Stay_alive=0, living=0;	// server stays running (ignores _PULSE_CODE_DISCONNECT request)
    living =1;
    while (living) {
	   // Do your MsgReceive's here now with the chid
       rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);

       if (rcvid == -1) { // Error condition, exit
           printf("\nFailed to MsgReceive\n");
           break;
       }

       // did we receive a Pulse or message?
       // for Pulses:
       if (rcvid == 0) { //  Pulse received, work out what type
		   printf("\nServer received a pulse from ClientID:%d ...\n", msg.ClientID);
		   printf("Pulse received:%d \n", msg.hdr.code);

           switch (msg.hdr.code) {
                case _PULSE_CODE_DISCONNECT:
                    printf("Pulse case:    %d \n", _PULSE_CODE_DISCONNECT);
                        // A client disconnected all its connections by running
                        // name_close() for each name_open()  or terminated
                    if( Stay_alive == 0) {
                        ConnectDetach(msg.hdr.scoid);
                        printf("\nServer was told to Detach from ClientID:%d ...\n", msg.ClientID);
                        living = 0; // kill while loop
                        continue;
                    }
                    else {
                        printf("\nServer received Detach pulse from ClientID:%d but rejected it ...\n", msg.ClientID);
                    }
                    break;
			   default:
				   // Some other pulse sent by one of your processes or the kernel
				   printf("\nServer got some other pulse after %d, msgnum\n", msgnum);
				   break;

           }
           continue;// go back to top of while loop
       }

       // for messages:
        if(rcvid > 0) { // if true then A message was received
            msgnum++;

            // If the Global Name Service (gns) is running, name_open() sends a connect message. The server must EOK it.
            if (msg.hdr.type == _IO_CONNECT ) {
                MsgReply( rcvid, EOK, NULL, 0 );
                msgnum--;
                continue;	// go back to top of while loop
            }

            // Some other I/O message was received; reject it
            if (msg.hdr.type > _IO_BASE && msg.hdr.type <= _IO_MAX ) {
                MsgError( rcvid, ENOSYS );
                continue;	// go back to top of while loop
            }

            // A message (presumably ours) received

            // put your message handling code here and assemble a reply message
            sprintf(replymsg.buf, "Message %d received", msgnum);
            printf("Server received data packet with value of '%d' from client (ID:%d), ", msg.data, msg.ClientID);
            fflush(stdout);
        
            printf("\n    -----> replying with: '%s'\n",replymsg.buf);
            MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));
        }

    }

   // Remove the attach point name from the file system (i.e. /dev/name/local/<myname>)
   name_detach(attach, 0);
   return EXIT_SUCCESS;
}
