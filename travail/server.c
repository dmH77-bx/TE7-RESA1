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
#include <poll.h>

#define FD_TAB_SIZE 128

#include "common.h"

int echo_server(int sockfd) {
	char buff[MSG_LEN];
	int msg_size;
	// Cleaning memory
	memset(buff, 0, MSG_LEN);
	// Receivig message size
	if (recv(sockfd, &msg_size, sizeof(msg_size), 0) <= 0) {
	    return 0;
	}
	printf("Received message size: %d\n", msg_size);
	// Receiving message
	if (recv(sockfd, buff, msg_size, 0) <= 0) {
	    return 0;
	}
	printf("Received: %s", buff);

	// Sending message size
	if (send(sockfd, &msg_size, sizeof(msg_size), 0) <= 0) {
		return 0;
	}
	printf("Size sent (%d)!\n", msg_size);
	// Sending message (ECHO)
	if (send(sockfd, buff, msg_size, 0) <= 0) {
		return 0;
	}
	printf("Message sent!\n");
    return 1;
}

int handle_bind(char *port) {
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if (getaddrinfo(NULL, port, &hints, &result) != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
		rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}
		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
			break;
		}
		close(sfd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not bind\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	return sfd;
}

int main(int argc, char *argv[]) {

	if (argc != 2)
	{
		printf("%s: <server_port>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	int sfd, connfd;
	sfd = handle_bind(argv[1]);
	if ((listen(sfd, SOMAXCONN)) != 0) {
		perror("listen()\n");
		exit(EXIT_FAILURE);
	}

    struct pollfd fds[FD_TAB_SIZE];

    fds[0].fd = sfd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    for (int i = 1; i < FD_TAB_SIZE; i++)
    {
        fds[i].fd = -1;
        fds[i].events = 0;
        fds[i].revents = 0;
    }

    while (1)
    {
        int nbfds = poll(fds, FD_TAB_SIZE, -1);
        if (nbfds == -1)
        {
            perror("Polling");
            continue;
        }


        for (int i = 0; i < FD_TAB_SIZE; i++)
        {
            // if activity on listening socket
            if (i == 0 && (fds[0].revents & POLLIN))
            {
                fds[0].revents = 0;
                connfd = accept(sfd, NULL, NULL);
                if (connfd < 0)
                {
                    perror("Accepting");
                    continue;
                }

                for (int j = 1; j < FD_TAB_SIZE; j++)
                {
                    if (fds[j].fd == -1)
                    {
                        fds[j].fd = connfd;
                        fds[j].events = POLLIN;
                        fds[j].revents = 0;
                        break;
                    }
                }
            }

            // if activity on client socket
            if (i != 0 && (fds[i].revents & POLLIN))
            {
                fds[i].revents = 0;
                if (echo_server(fds[i].fd) == 0)
                {
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    fds[i].events = 0;
                    fds[i].revents = 0;
                    printf("Connection ended\n");
                    continue;
                }

            }
        }
    }

	close(sfd);
	return EXIT_SUCCESS;
}

