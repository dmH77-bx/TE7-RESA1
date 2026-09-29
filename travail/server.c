#define _DEFAULT_SOURCE
#include "msg_struct.h"
#include "common.h"
#include "client_list.h"


#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>

#define MAX_CLIENTS 128
#define NI_MAXHOST 1025
#define NI_MAXSERV 32


int setup_listening_socket(const char *port) {
	int listen_fd;
	int ret_value;
	struct addrinfo hints, *result, *rp;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // To listen on all interfaces
	int error = getaddrinfo(NULL, port, &hints, &result);
	if (error != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		listen_fd = socket(rp->ai_family, rp->ai_socktype,
		rp->ai_protocol);
		if (listen_fd == -1) {
			continue;
		}

		int opt = 1; // 1 to activate the option
		setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
			break;
		}
		close(listen_fd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not bind\n");
		freeaddrinfo(result);
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	printf("TCP listening socket created.\n");
	printf("Socket bound to port %d.\n", atoi(port));

	ret_value = listen(listen_fd, 20);
	die(ret_value, "listen");
	printf("Listening for client connections.\n");
	return listen_fd;
}

void accept_and_insert_client(int listen_fd, struct pollfd poll_fds[MAX_CLIENTS], struct client_info **clients) {
	struct sockaddr_storage client_address;
	socklen_t client_address_length = sizeof(client_address);
	char host[NI_MAXHOST];
	char port[NI_MAXSERV];

	int client_fd = accept(listen_fd, (struct sockaddr *)&client_address, &client_address_length);
	die(client_fd, "accept");
	getnameinfo((struct sockaddr *)&client_address, client_address_length, host, sizeof(host), port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);
	// The function getnameinfo() allows implicitly to use the functions inet_ntoa() and ntohs
	int slot;

	for (slot = 1; slot < MAX_CLIENTS; slot++) {
		if (poll_fds[slot].fd < 0) {
			if (client_list_add(clients, client_fd, &client_address) < 0) {
				close(client_fd);
				die(-1, "malloc client information");
			}
			poll_fds[slot].fd = client_fd;
			poll_fds[slot].events = POLLIN;
			poll_fds[slot].revents = 0;
			printf("Accepted client %s:%s on slot %d.\n", host, port, slot);
			break;
		}
	}
	if (slot == MAX_CLIENTS) {
		fprintf(stderr, "Client limit reached. Closing the new connection.\n");
		close(client_fd);
	}
}

// Return 1 when the client should be disconnected, 0 otherwise
int handle_client_message(struct client_info *clients, int client_fd) {
	struct message message;
	char payload[MAX_PAYLOAD_SIZE + 1];

	if (receive_structure_and_payload(client_fd, &message, payload, MAX_PAYLOAD_SIZE) == 0) {
		return 1;
	}

	fprintf(stdout, "Client %d [%s] nick_sender=\"%s\" infos=\"%s\" payload=\"%s\"\n", client_fd, msg_type_str[message.type], message.nick_sender, message.infos, payload);

	if (empty_nickname(clients, client_fd) && message.type != NICKNAME_NEW) {
		strcpy(payload, "Please enter your nickname first");
		message.pld_len = strlen(payload);
		message.type = NICKNAME_NEW;
	}
	else {
		switch (message.type)
		{
			case ECHO_SEND:
				break;
			case NICKNAME_NEW:
				if (valid_nickname(message.infos, NICK_LEN - 1) == 0) {
					strcpy(payload, "Invalid nickname");
					message.pld_len = strlen(payload);
					break;
				}
				if (nickname_exists(clients, client_fd, message.infos)) {
					strcpy(payload, "Nickname already taken");
					message.pld_len = strlen(payload);
					break;
				}
				if (set_nickname(clients, client_fd, message.infos) == 0) {
					strcpy(payload, "Impossible to change/set nickname");
					message.pld_len = strlen(payload);
					break;
				}
				strcpy(payload, "Welcome to the chat ");
				strcat(payload, message.infos);
				message.pld_len = strlen(payload);
				break;
			case NICKNAME_LIST:
				strcpy(payload, "Online users are\n");
				nickname_list(clients, payload, MAX_PAYLOAD_SIZE);
				message.pld_len = strlen(payload);
				break;
			case NICKNAME_INFOS:
				strcpy(payload, message.infos);
				strcat(payload, " connected since ");
				time_t t = get_time(clients, message.infos);
				struct tm *date = localtime(&t);
				char connection_time[64];
				strftime(connection_time, 64, "%Y/%m/%d@%H:%M", date);
				strcat(payload, connection_time);
				strcat(payload, " with IP address ");
				char address[NI_MAXHOST];
				char port[NI_MAXSERV];
				get_address_and_port(clients, message.infos, address, port);
				strcat(payload, address);
				strcat(payload, " and port number ");
				strcat(payload, port);
				strcat(payload, "\n");
				message.pld_len= strlen(payload);
				break;
			default:
				return 0;
		}
	}
	if (message.type == NICKNAME_NEW) {
		get_nickname(clients, client_fd, message.nick_sender);
	}
	if (send_structure_and_payload(client_fd, &message, payload) == 0) {
		return 1;
	}
	return 0;
}

void server_poll_loop(int listen_fd, struct pollfd poll_fds[MAX_CLIENTS], struct client_info **clients) {
	int running = 1;

	// Slot 0 is the listener. The other slots contain client sockets
	for (int i = 0; i < MAX_CLIENTS; i++) {
		poll_fds[i].fd = -1;
		poll_fds[i].events = 0;
		poll_fds[i].revents = 0;
	}
	poll_fds[0].fd = listen_fd;
	poll_fds[0].events = POLLIN;

	// execute server logic
	while (running) {
		int ready = poll(poll_fds, MAX_CLIENTS, -1);
		die(ready, "poll");

		if ((poll_fds[0].revents & POLLIN) != 0) {
			accept_and_insert_client(listen_fd, poll_fds, clients);
		}

		for (int slot = 1; slot < MAX_CLIENTS; slot++) {
			short returned_events = poll_fds[slot].revents;
			int close_connection = 0;
			if (poll_fds[slot].fd < 0) {
				continue;
			}

			if ((returned_events & POLLIN) != 0) {
				close_connection = handle_client_message(*clients, poll_fds[slot].fd);
			}
			if ((returned_events & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
				close_connection = 1;
			}
			if (close_connection) {
				int client_fd = poll_fds[slot].fd;
				close(client_fd);
				client_list_remove(clients, client_fd);
				poll_fds[slot].fd = -1;
				poll_fds[slot].events = 0;
				poll_fds[slot].revents = 0;
			}
		}
		if ((poll_fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
			running = 0;
		}
	}

	// Cleaning up: close all client sockets and free the client list
	for (int slot = 1; slot < MAX_CLIENTS; slot++) {
		if (poll_fds[slot].fd >= 0) {
			close(poll_fds[slot].fd);
			poll_fds[slot].fd = -1;
		}
	}
	client_list_destroy(clients);
}

int main(int argc, char **argv) {
	struct pollfd poll_fds[MAX_CLIENTS];
	struct client_info *clients = NULL;
	const char *port;
	int listen_fd;

	if (argc != 2) {
		fprintf(stderr, "Usage: ./server <server_port>\n");
		return EXIT_FAILURE;
	}
	port = argv[1];
	if (atoi(port) < 1 || atoi(port) > 65535) {
		fprintf(stderr, "Invalid port\n");
		return EXIT_FAILURE;
	}

	listen_fd = setup_listening_socket(port);
	server_poll_loop(listen_fd, poll_fds, &clients);
	close(listen_fd);
	return EXIT_SUCCESS;
}
