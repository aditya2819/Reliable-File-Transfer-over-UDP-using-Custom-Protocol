#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

#include "include/helper.h"
#include "include/consts.h"
#include "include/receiver.h"
#include "include/sender.h"

// Global configuration variables
unsigned int selfport;
unsigned int recvport;
unsigned int partsize;
unsigned int batchsize;

// Initialize global variables based on port values
void init_globals(int p1, int p2)
{
	selfport = p1;                     // Port used by this instance
	recvport = p2;                     // Port of the receiver
	partsize = BUFLEN - 10;           // Max size of one data part
	batchsize = partsize * PARTSINBATCH; // Total size for a batch of parts
}

// Display current configuration values
void show_globals()
{
	printf("self port %u\n", selfport);
	printf("receiver port %u\n", recvport);
	printf("partsize %u\n", partsize);
	printf("batchsize %u\n", batchsize);
}

// Message-related buffers and lengths
unsigned int sendmsglen;
unsigned int recvmsglen;

unsigned char sendbuf[BUFLEN];
unsigned char recvbuf[BUFLEN];

unsigned char op; // Current operation code

// Socket and address structures
int sockfd;
struct sockaddr_in receiver_addr, self_addr;

// Create and bind a UDP socket, configure receiver address
void config_socket(char *r_addr)
{
	struct in_addr addr;

	memset(&receiver_addr, 0, sizeof(receiver_addr));
	memset(&self_addr, 0, sizeof(self_addr));

	// Create UDP socket
	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf("Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	// Validate and convert receiver IP address
	if(inet_aton(r_addr, &addr) == 0)
	{
		printf("Invalid receiver address\n");
		close(sockfd);
		exit(EXIT_FAILURE);
	}

	// Set up receiver address
	receiver_addr.sin_family = AF_INET;
	receiver_addr.sin_port = htons(recvport);
	receiver_addr.sin_addr = addr;

	// Set up self address
	self_addr.sin_family = AF_INET;
	self_addr.sin_port = htons(selfport);

	// Bind the socket to self address
	if(bind(sockfd, (struct sockaddr *)&self_addr, (socklen_t)sizeof(self_addr)) < 0)
	{
		printf("Bind failed\n");
		close(sockfd);
		exit(EXIT_FAILURE);
	}
}

// Send a message to the preconfigured receiver address
int send_msg(unsigned char *msg, int msglen)
{
	int sentbytes;

	sentbytes = sendto(sockfd, msg, msglen, 0, (struct sockaddr *)&receiver_addr, (socklen_t)sizeof(receiver_addr));
	if(sentbytes == -1)
	{
		printf("send_msg: %s", strerror(errno));
		sentbytes = 0;
	}

	return sentbytes;
}

// Receive a message (non-blocking). Stores message in buffer.
int recv_msg(unsigned char *msg)
{
	int recvedbytes;

	recvedbytes = recvfrom(sockfd, msg, partsize, MSG_DONTWAIT, NULL, NULL);

	if(recvedbytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		recvedbytes = 0; // No message received
	else if(recvedbytes == -1)
	{
		printf("recv_msg: %s", strerror(errno));
		recvedbytes = 0;
	}

	return recvedbytes;
}

// Check if user input is a valid "send" command with a valid filename
int isvalid_send_cmd(char *cmd)
{
	char str[FILENMLEN];
	int res = 0, tryno = 0, fileidx = 5;

	memmove(str, cmd, 4);  // Copy first 4 chars
	str[4] = '\0';

	// Check if command starts with "send"
	if(strcmp(str, "send") == 0)
	{
		memmove(str, &(cmd[fileidx]), FILENMLEN); // Extract filename

		// Allow up to 3 attempts to correct filename
		while((tryno < 3) && (res == 0))
        {
		    if(isvalid_file(str) == 1)
			{	
				res = 1;
				memmove(&(cmd[fileidx]), str, FILENMLEN);
			}
			else
			{
				printf("Seems like you have entered a wrong filename to send\n");
				printf("Enter file to send: ");
				scanf("%s", str);
			}
			tryno += 1;
		}
	}
	return res;
}

// Entry point of the program
int main(int c, char* argv[])
{
	char addr[20] = "127.0.0.1"; // Default receiver IP (localhost)
	char cmd[32];

	// Initialize ports from command line arguments
	init_globals(atoi(argv[1]), atoi(argv[2]));

	// Setup the socket and bind it
	config_socket(addr);

	// Read command from user input
	fgets(cmd, sizeof(cmd), stdin);
	cmd[strlen(cmd)-1] = '\0'; // Remove newline character

	// Check for valid command and execute
	if(isvalid_send_cmd(cmd) == 1)
		send_file(&cmd[5]);	
	else if(strcmp(cmd, "receive") == 0)
		receive_file();
	else if((!strcmp(cmd, "exit")) == 0)
		printf("Invalid command\n");

	return 0;

	close(sockfd);
}
