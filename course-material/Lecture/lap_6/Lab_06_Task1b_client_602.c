/*
    RMIT University Vietnam
    Course: EEET2588 Real-Time System Engineering
    Semester: 2026-2
    Author: Hoang Minh Thang
    ID: s3999925
    Week 7 - Lab Exercise 6 - Task 1B
    Due date: 28/08/2026
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/dispatch.h>

// Define where the "QNet" attach point is located
#define QNET_ATTACH_POINT "/net/VM_x86_Target01/dev/name/local/thang"  // change myname to the same name used on the client side
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
    int data;          // our data <-- This is what we are here for
} my_data;

typedef struct {
    msg_header_t hdr;   // Custom header
    char buf[BUF_SIZE]; // Message to send back to send back to other thread
} my_reply;

// prototypes
int client(char *sname);

int main(int argc, char *argv[]) {
    // Task 1B: Client code with ID 602
	printf("Welcome to the Task 1B Client with ID 602!\n");
	printf("This is Client 602 running\n");

    int ret = 0;
    ret = client(QNET_ATTACH_POINT);  // use for QNet connection "myname"
	printf("Main (client) Terminated....\n");
	return ret;
}

/*** Client code ***/
int client(char *sname) {
    my_data msg;
    my_reply reply; // replymsg structure for sending back to client

    msg.ClientID = 602;      // client unique number to execute
    msg.hdr.type = 0x22;     // We would have pre-defined data to stuff here
    int server_coid;
    int index = 0;

    printf("  ---> Trying to connect to server named: %s\n", sname);
    if ((server_coid = name_open(sname, 0)) == -1) {
        printf("\n    ERROR, could not connect to server!\n\n");
        return EXIT_FAILURE;
    }

    printf("Connection established to: %s\n", sname);

    // Do whatever work you wanted with server connection
    for (index=0; index < 5; index++) {// send data packets
    	// set up data packet
    	msg.data=10+index;

    	// the data we are sending is in msg.data
        printf("Client (ID:%d), sending data packet with the integer value: %d \n", msg.ClientID, msg.data);
        fflush(stdout);

        if (MsgSend(server_coid, &msg, sizeof(msg), &reply, sizeof(reply)) == -1) {
            printf(" Error data '%d' NOT sent to server\n", msg.data);
            break;
        }
        else { // now process the reply
            printf("   -->Reply is: '%s'\n", reply.buf);
        }
    }

    // Close the connection
    printf("\n Sending message to server to tell it to close the connection\n");
    name_close(server_coid);

    return EXIT_SUCCESS;
}
