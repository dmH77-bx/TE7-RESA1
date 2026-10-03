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
#include <fcntl.h>

#define NI_MAXHOST 1025
#define NI_MAXSERV 32
#define MAX_LINE_SIZE (PROTO_MAX_PAYLOAD + 200)
#define FILEPATH_LEN 4096

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
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
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


int reject_transfer(int socket_fd, char *my_nickname, char *sender_nickname) {
	struct message message = {0};
	message.type = FILE_REJECT;
	strcpy(message.nick_sender, my_nickname);
	strcpy(message.infos, sender_nickname);
	message.pld_len = 0;
	return send_structure_and_payload(socket_fd, &message, NULL);
}

// Return 1 to keep running, or 0 if the server disconnects or sends an invalid message
int read_server_message(int socket_fd, char *my_nickname, char *sender_nickname, char *filename, char *filepath_to_send) {
	struct message message;
	char payload[PROTO_MAX_PAYLOAD + 1];

	if (receive_structure_and_payload(socket_fd, &message, payload, PROTO_MAX_PAYLOAD) == 0) {
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
			fprintf(stdout, "[%s] : %s\n", message.nick_sender, payload);
			break;
		case UNICAST_SEND:
			if (message.nick_sender[0] == '\0') {
				fprintf(stdout, "[Server] : %s\n", payload);
				break;
			}
			fprintf(stdout, "[%s] : %s\n", message.nick_sender, payload);
			break;
		case FILE_REQUEST:
			if (message.nick_sender[0] == '\0') {
				fprintf(stdout, "[Server] : %s\n", payload);
				break;
			}
			if (strcmp(sender_nickname, "") != 0) {
				return reject_transfer(socket_fd, my_nickname, message.nick_sender);
			}
			if (strlen(payload) >= INFOS_LEN) {
				return reject_transfer(socket_fd, my_nickname, message.nick_sender);
			}
			strcpy(sender_nickname,  message.nick_sender);
			strcpy(filename, payload);
			fprintf(stdout, "%s wants you to accept the transfer of the file named \"%s\". Do you accept? [Y/N]\n", message.nick_sender, payload);
			break;
		case FILE_ACCEPT: {
			fprintf(stdout, "%s accepted file transfer.\n", message.nick_sender);
			fprintf(stdout, "Connecting to %s and sending the file...\n", message.nick_sender);
			struct addrinfo hints, *result, *rp;
			int peer_fd;
			char *port_dest;
			char *separator = strrchr(payload, ':');
			if (separator == NULL) {
				fprintf(stderr, "Invalid local address\n");
				return 1;
			}
			*separator = '\0';
			port_dest = separator + 1;

			memset(&hints, 0, sizeof(struct addrinfo));
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;
			int error = getaddrinfo(payload, port_dest, &hints, &result);
			if (error != 0) {
				fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error)); 
				return 1;
			}

			for (rp = result; rp != NULL; rp = rp->ai_next) {
				peer_fd = socket(rp->ai_family, rp->ai_socktype,rp->ai_protocol);
				if (peer_fd == -1) {
					continue;
				}
				if (connect(peer_fd, rp->ai_addr, rp->ai_addrlen) != -1) {
					break;
				}
				close(peer_fd);
			}
			if (rp == NULL) {
				fprintf(stderr, "Could not connect\n");
				freeaddrinfo(result);
				return 1;
			}
			freeaddrinfo(result);
			break; }
		case FILE_REJECT:
			if (message.nick_sender[0] == '\0') {
				fprintf(stdout, "[Server] : %s\n", payload);
				break;
			}
			fprintf(stdout, "%s rejected file transfer.\n", message.nick_sender);
			break;
		default:
			fprintf(stderr, "Invalid message type: %s\n", msg_type_str[message.type]);
			break;
	}
	return 1;
}


// Return 1 to keep running, or 0 when stdin closes or the user quits
int get_and_send_user_message(int socket_fd, char *my_nickname, char *sender_nickname, char *filename, char *filepath_to_send) {
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
		pseudo_target = msg_line + 7; // To get the targeted pseudo

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
		if (strlen(payload) > PROTO_MAX_PAYLOAD) {
            fprintf(stderr, "Payload too long\n");
            return 1; 
        }
        message.pld_len = (int)strlen(payload);
		return send_structure_and_payload(socket_fd, &message, payload);
	}

	if (strncmp(msg_line, "/msg ", 5) == 0) {
		pseudo_target = msg_line + 5;
		char *space = strchr(pseudo_target, ' ');
		if (space == NULL) {
			fprintf(stderr, "Invalid form: /msg <pseudo> <message>\n");
			return 1;
		}
		*space = '\0';
		message_unicast = space + 1;

		if (valid_nickname(pseudo_target, NICK_LEN - 1) == 0) {
			fprintf(stderr, "%s: Invalid nickname\n", msg_type_str[UNICAST_SEND]);
			return 1;
		}

		message.type = UNICAST_SEND;
		strcpy(message.infos, pseudo_target);
		payload = message_unicast;
		if (strlen(payload) > PROTO_MAX_PAYLOAD) {
			fprintf(stderr, "Payload too long\n");
			return 1;
		}
		message.pld_len = (int)strlen(payload);
		return send_structure_and_payload(socket_fd, &message, payload);
	}

	if (strncmp(msg_line, "/send ", 6) == 0) {
		message.type = FILE_REQUEST;
		pseudo_target = msg_line + 6;
		char *space = strchr(pseudo_target, ' ');
		if (space == NULL) {
			fprintf(stderr, "Invalid form: /send <pseudo> <filename>\n");
			return 1;
		}
		*space = '\0';
		char *filepath = space + 1; // cut the string to separate nickname from filepath
		// Remove the double quotes and extract the filename from the whole filepath
		if (filepath[0] == '"') {
			filepath++;
		}
		char *quote = strrchr(filepath, '"');
		if (quote != NULL && quote[1] == '\0') {
			*quote = '\0';
		}
		if (filepath[0] == '\0') {
			fprintf(stderr, "Invalid filepath\n");
			return 1;
		}

		char *file_basename = filepath;
		char *slash = strrchr(filepath, '/');
		if (slash != NULL) {
			file_basename = slash + 1;
		}
		if (file_basename[0] == '\0') {
			fprintf(stderr, "Invalid filepath\n");
			return 1;
		}
		if (valid_nickname(pseudo_target, NICK_LEN - 1) == 0) {
			fprintf(stderr, "%s: Invalid nickname\n", msg_type_str[FILE_REQUEST]);
			return 1;
		}

		if (strlen(filepath) >= FILEPATH_LEN || strlen(file_basename) >= INFOS_LEN) {
			fprintf(stderr, "Filepath too long\n");
			return 1;
		}

		strcpy(filepath_to_send, filepath); 
		strcpy(message.infos, pseudo_target);
		payload = file_basename;
		message.pld_len = (int)strlen(payload);
		return send_structure_and_payload(socket_fd, &message, payload);
	}

	if ((strcmp(msg_line, "Y") == 0) && (strcmp(sender_nickname, "") != 0)) {
		int listen_fd = -1;
		int ret;
		struct addrinfo hints, *result, *rp;

		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_INET6;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;
		int error = getaddrinfo(NULL, "0", &hints, &result);
		if (error != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
			ret = reject_transfer(socket_fd, my_nickname, sender_nickname);
			strcpy(sender_nickname, "");
			return ret;
		}
		for (rp = result; rp != NULL; rp = rp->ai_next) {
			listen_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
			if (listen_fd == -1) {
				continue;
			}
			if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
				break;
			}
			close(listen_fd);
		}
		freeaddrinfo(result);
		if (rp == NULL) {
			fprintf(stderr, "Could not bind\n");
			ret = reject_transfer(socket_fd, my_nickname, sender_nickname);
			strcpy(sender_nickname, "");
			return ret;
		}
		if (listen(listen_fd, 1) < 0) {
			perror("listen");
			close(listen_fd);
			ret = reject_transfer(socket_fd, my_nickname, sender_nickname);
			strcpy(sender_nickname, "");
			return ret;
		}

		char host[NI_MAXHOST];
		char port[NI_MAXSERV];
		struct sockaddr_storage addr;
		socklen_t len = sizeof(addr);
		getsockname(listen_fd, (struct sockaddr *)&addr, &len);       // fill addr with the local address (IP and port) the socket is bound to
		getnameinfo((struct sockaddr *)&addr, len, NULL, 0, port, sizeof(port), NI_NUMERICSERV); // to get the port#

		len = sizeof(addr);
		getsockname(socket_fd, (struct sockaddr *)&addr, &len);
		getnameinfo((struct sockaddr *)&addr, len, host, sizeof(host), NULL, 0, NI_NUMERICHOST); // to get the IP@

		char accept_payload[NI_MAXHOST + NI_MAXSERV + 2];
		strcpy(accept_payload, host);
		strcat(accept_payload, ":");
		strcat(accept_payload, port);

		message.type = FILE_ACCEPT;
		strcpy(message.infos, sender_nickname);
		message.pld_len = strlen(accept_payload);
		if (send_structure_and_payload(socket_fd, &message, accept_payload) == 0) {
			close(listen_fd);
			return 0;
		}

		int sender_fd = accept(listen_fd, NULL, NULL);
		close(listen_fd);
		if (sender_fd == -1) {
			perror("accept");
			strcpy(sender_nickname, "");
			return 1;
		}

		// 3) receive the file, send FILE_ACK, close(sender_fd)
		// 4) empty sender_nickname and filename

		return 1;
	}

	if ((strcmp(msg_line, "N") == 0) && (strcmp(sender_nickname, "") != 0)) {
		int ret;
		ret = reject_transfer(socket_fd, my_nickname, sender_nickname);
		strcpy(sender_nickname, "");
		strcpy(filename, "");
		return ret;
	}

	if (strcmp(msg_line, "/quit") == 0) {
		return 0;
	}

	payload = msg_line;
	if (strlen(payload) > PROTO_MAX_PAYLOAD) {
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
	char sender_nickname[NICK_LEN] = {0};
	char filename[INFOS_LEN] = {0}; // Limit the filename at INFOS_LEN since for FILE_SEND, infos will contain the filename
	char filepath_to_send[FILEPATH_LEN] = {0}; 
	struct pollfd watched[2];
	int running = 1;

	// Initialize once; poll() fills revents after each call
	watched[0].fd = STDIN_FILENO;
	watched[0].events = POLLIN;
	watched[1].fd = socket_fd;
	watched[1].events = POLLIN;

	while (running) {
		int ready = poll(watched, 2, -1);
		die(ready, "poll");

		if ((watched[1].revents & POLLIN) != 0) {
			running = read_server_message(socket_fd, my_nickname, sender_nickname, filename, filepath_to_send);
		}

		if (running && (watched[0].revents & POLLIN) != 0) {
			running = get_and_send_user_message(socket_fd, my_nickname, sender_nickname, filename, filepath_to_send);
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
