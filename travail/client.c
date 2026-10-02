#define _DEFAULT_SOURCE
#include "msg_struct.h"
#include "common.h"


#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define NI_MAXHOST 1025
#define NI_MAXSERV 32
#define MAX_LINE_SIZE 4324


int setup_connection(const char *server_ip, const char *server_port) {
	struct addrinfo hints, *result, *rp;
	int socket_fd;
	char host[NI_MAXHOST]; // gameinfo will stock the IP@ in host
	char port[NI_MAXSERV]; // gameinfo will stock the port# in port

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	int error = getaddrinfo(server_ip, server_port, &hints, &result); // This function creates a linked list
	if (error != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}

	printf("Using server IP address %s.\n", server_ip);

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		socket_fd = socket(rp->ai_family, rp->ai_socktype,rp->ai_protocol);
		if (socket_fd == -1) {
			continue;
		}
		if (connect(socket_fd, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;
		}
		close(socket_fd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not connect\n");
		freeaddrinfo(result);
		exit(EXIT_FAILURE);
	}

	getnameinfo(rp->ai_addr, rp->ai_addrlen, host, sizeof(host), port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);
	freeaddrinfo(result);
	printf("TCP socket created.\n");

	printf("Connected to %s:%s.\n", host, port);
	return socket_fd;
}


// Return 1 to keep running, or 0 if the server disconnects or sends an invalid message
int read_server_message(int socket_fd, char *my_nickname) {
	struct message message;
	char payload[MAX_PAYLOAD_SIZE + 1];

	if (receive_structure_and_payload(socket_fd, &message, payload, MAX_PAYLOAD_SIZE) == 0) {
		return 0;
	}

	switch (message.type) {
		case ECHO_SEND:
			fprintf(stdout, "%s\n", payload);
			break;
		case NICKNAME_NEW:
			strcpy(my_nickname, message.nick_sender);
			fprintf(stdout, "[Server] : %s\n", payload);
			break;
		case NICKNAME_LIST:
			fprintf(stdout, "[Server] : %s", payload);
			break;
		case NICKNAME_INFOS:
			fprintf(stdout, "[Server] : %s", payload);
			break;
		case BROADCAST_SEND:
			fprintf(stdout, "%s\n", payload);
			break;
		case UNICAST_SEND:
			fprintf(stdout, "%s\n", payload);
			break;
		default:
			fprintf(stderr, "Invalid message type: %s\n", msg_type_str[message.type]);
			break;
	}

	return 1;
}

// Return 1 to keep running, or 0 when stdin closes or the user quits
int get_and_send_user_message(int socket_fd, char *my_nickname) {
	struct message message = {0};
	char *payload;
	ssize_t bytes_read;
	char msg_line[MAX_LINE_SIZE + 1];
	char *pseudo;
	char *pseudo_target;
	char *message_broad;
	char *message_unicast;

	bytes_read = read(STDIN_FILENO, msg_line, MAX_LINE_SIZE);
	die((int)bytes_read, "read stdin");
	if (bytes_read == 0) {
		return 0;
	}

	if (msg_line[bytes_read - 1] == '\n') {
		bytes_read--;
	}

	strcpy(message.nick_sender, my_nickname);
	msg_line[bytes_read] = '\0';
	if (strcmp(msg_line, "/who") == 0) {
		message.pld_len = 0;
		message.type = NICKNAME_LIST;
		return send_structure_and_payload(socket_fd, &message, NULL);
	}
	if (strncmp(msg_line, "/nick ", 6) == 0) {
		pseudo = msg_line + 6; // To get the pseudo

		if (valid_nickname(pseudo, NICK_LEN - 1) == 0) {
			fprintf(stderr, "%s: Invalid nickname\n", msg_type_str[NICKNAME_NEW]);
			return 1; // To refuse the nickname but keep the client connected
		}

		message.type = NICKNAME_NEW;
		strcpy(message.infos, pseudo);
		message.pld_len = 0;
		return send_structure_and_payload(socket_fd, &message, NULL);
	}

	if (strcmp(msg_line, "/nick") == 0) {
		fprintf(stderr, "Nickname missing\n");
		return 1;
	}

	if (strncmp(msg_line, "/whois ", 7) == 0) {
		pseudo_target = msg_line + 7; // To get the pseudo targeted

		if (valid_nickname(pseudo_target, NICK_LEN - 1) == 0) {
			fprintf(stderr, "%s: Invalid nickname\n", msg_type_str[NICKNAME_INFOS]);
			return 1; // To refuse the nickname but keep the client connected
		}

		message.type = NICKNAME_INFOS;
		strcpy(message.infos, pseudo_target);
		message.pld_len = 0;
		return send_structure_and_payload(socket_fd, &message, NULL);
	}

	if (strncmp(msg_line, "/msgall ", 8) == 0) {
		message_broad = msg_line + 8;
		message.type = BROADCAST_SEND;
		payload = message_broad;
		if (strlen(payload) > MAX_PAYLOAD_SIZE) {
            fprintf(stderr, "Payload too long\n");
            return 1; 
        }
        message.pld_len = (int)strlen(payload);
		return send_structure_and_payload(socket_fd, &message, payload);
	}

	if (strncmp(msg_line, "/msg ", 5) == 0) {
		message_unicast = msg_line + 5;
		message.type = UNICAST_SEND;
		payload = message_unicast;
		if (strlen(payload) > MAX_PAYLOAD_SIZE) {
            fprintf(stderr, "Payload too long\n");
            return 1; 
        }
        message.pld_len = (int)strlen(payload);
		return send_structure_and_payload(socket_fd, &message, payload);
	}

	if (strcmp(msg_line, "/quit") == 0) {
		return 0;
	}

	payload = msg_line;
	if (strlen(payload) > MAX_PAYLOAD_SIZE) {
		fprintf(stderr, "Payload too long: %d\n", (int)strlen(payload));
		return 1; // To refuse the message but keep the client connected
	}
	message.pld_len = (int)strlen(payload);
	message.type = ECHO_SEND;

	if (send_structure_and_payload(socket_fd, &message, payload) == 0) {
		return 0;
	}

	return 1;
}


void client_poll_loop(int socket_fd) {
	char my_nickname[NICK_LEN] = {0};
	struct pollfd watched[2];
	int running = 1;

	/* Initialize once; poll() fills revents after each call. */
	watched[0].fd = STDIN_FILENO;
	watched[0].events = POLLIN;
	watched[1].fd = socket_fd;
	watched[1].events = POLLIN;

	while (running) {
		int ready = poll(watched, 2, -1);
		die(ready, "poll");

		if ((watched[1].revents & POLLIN) != 0) {
			running = read_server_message(socket_fd, my_nickname);
		}

		if (running && (watched[0].revents & POLLIN) != 0) {
			running = get_and_send_user_message(socket_fd, my_nickname);
		}

		if ((watched[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0 || (watched[1].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
			running = 0;
		}
	}
}

int main(int argc, char **argv) {
	int socket_fd;

	if (argc != 3) {
		fprintf(stderr, "Usage: ./client <server_ip@> <server_port>\n");
		return EXIT_FAILURE;
	}
	socket_fd = setup_connection(argv[1], argv[2]);
	if (socket_fd < 0) {
		return EXIT_FAILURE;
	}
	client_poll_loop(socket_fd);
	close(socket_fd);
	return EXIT_SUCCESS;
}
