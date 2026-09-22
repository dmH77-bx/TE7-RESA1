#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"

void echo_client(int sockfd) {
	char buff[MSG_LEN];
	int msg_size;
	int n;
	while (1) {
		// Cleaning memory
		memset(buff, 0, MSG_LEN);
		// Getting message from client
		printf("Message: ");
		n = 0;
		while ((buff[n++] = getchar()) != '\n') {} // trailing '\n' will be sent
		msg_size = strlen(buff);
		// Sending message size
		if (send(sockfd, &msg_size, sizeof(msg_size), 0) <= 0) {
			break;
		}
		printf("Message size sent (%d)!\n", msg_size);

		// Sending message (ECHO)
		if (send(sockfd, buff, msg_size, 0) <= 0) {
			break;
		}
		printf("Message sent!\n");
		// Cleaning memory
		memset(buff, 0, MSG_LEN);
		msg_size = 0;
		// Received message size
		if (recv(sockfd, &msg_size, sizeof(msg_size), 0) <= 0) {
	    	break;
		}
		printf("Received size: %d\n", msg_size);
		// Receiving message
		if (recv(sockfd, buff, msg_size, 0) <= 0) {
			break;
		}
		printf("Received: %s", buff);
	}
}

int handle_connect(char *addr, char *port) {
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(addr, port, &hints, &result) != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}
		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;
		}
		close(sfd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	return sfd;
}

int main(int argc, char *argv[]) {
	if (argc != 3)
	{
		printf("%s: <server_name> <server_port>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	int sfd;
	sfd = handle_connect(argv[1], argv[2]);
	echo_client(sfd);
	close(sfd);
	return EXIT_SUCCESS;
}

