#include "msg_struct.h"
#include "common.h"


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>


void die(int val, char *msg) {
	if (val < 0) {
		perror(msg);
		exit(EXIT_FAILURE);
	}
}

int read_from_socket(int fd, void *buf, size_t msg_size) {
	size_t total = 0;
	char *cursor = buf;

	while (total < msg_size) {
		ssize_t n = read(fd, cursor + total, msg_size - total);
		if (n < 0) {
			perror("read");
			return 0;
		}
		if (n == 0) {
			return 0;
		}
		total += (size_t)n;
	}
	return (int)total;
}

int write_in_socket(int fd, void *buf, size_t msg_size) {
	size_t total = 0;
	char *cursor = buf;

	while (total < msg_size) {
		ssize_t n = send(fd, cursor + total, msg_size - total, MSG_NOSIGNAL);
		if (n < 0) {
			perror("send");
			return 0;
		}
		if (n == 0) {
			return 0;
		}
		total += (size_t)n;
	}
	return (int)total;
}

int send_structure_and_payload(int fd, struct message *message, char *payload) {
	int ret;
	int pld_len = message->pld_len;
	if (pld_len < 0 || pld_len > PROTO_MAX_PAYLOAD || (pld_len > 0 && payload == NULL)) {
		return 0;
	}
	ret = write_in_socket(fd, message, sizeof(*message));
	if (ret == 0) {
		return 0;
	}
	if (pld_len > 0) {
		ret = write_in_socket(fd, payload, pld_len);
		if (ret == 0) {
			return 0;
		}
	}
	return 1;
}


int receive_structure_and_payload(int fd, struct message *message, char *payload, int max_size) {
	int ret;
	ret = read_from_socket(fd, message, sizeof(*message));
	if (ret == 0) {
		return 0;
	}

	enum msg_type type = message->type;
	if (type < NICKNAME_NEW  || type > FILE_ACK) {
		return 0;
	}

	if (memchr(message->nick_sender, '\0', NICK_LEN) == NULL || memchr(message->infos, '\0', INFOS_LEN) == NULL) {
		return 0;
	}

	int pld_len = message->pld_len;
	if (pld_len > max_size || pld_len < 0) {
		fprintf(stderr, "Invalid pld_len %d for %s\n", pld_len, msg_type_str[type]);
		return 0;
	}
	if (pld_len > 0) {
		ret = read_from_socket(fd, payload, pld_len);
		if (ret == 0) {
			return 0;
		}
	}
	payload[pld_len] = '\0';

	return 1;
}

// Return 1 if nickname is valid; 0 if it's not
int valid_nickname(char *pseudo, size_t max_size) {
	if (pseudo[0] == '\0') {
		return 0;
	}

	if (strlen(pseudo) > max_size) {
		return 0;
	}
	
	int i = 0;
	while (pseudo[i] != '\0') {
		if (!isalnum(pseudo[i])) {
			return 0;
		}
		i++;
	}

	return 1;
}