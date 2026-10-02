#define _DEFAULT_SOURCE
#include "client_list.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <netdb.h>

#define NICK_LEN 128

struct client_info {
	int fd;
	char nickname[NICK_LEN];
	struct sockaddr_storage address;
	time_t connection_time;
	struct client_info *next;
};


// Returns fd if nickname exists, -1 if not
int get_fd_from_nick(struct client_info *clients, char *nick ) {
        struct client_info *cursor = clients;
        int fd;
        while (cursor != NULL) {
                if (strcmp(cursor->nickname, nick) == 0) {
                        fd = cursor->fd;
                        return fd;
                }
                cursor = cursor->next;
        }
        return -1;
}

// Add the fds of clients into list excluding that of the sender and return the number of fds found
int fd_list_without_sender(struct client_info *clients, int fd, int *list_fd, int max_size) {
	struct client_info *cursor = clients;
	int count = 0;

	while (cursor != NULL && count < max_size) {
			if (cursor->fd != fd) {
				list_fd[count] = cursor->fd;
				count++;
			}
		cursor = cursor->next;
	}
	return count;
}



// Copy address and #port into address and port
void get_address_and_port(struct client_info *clients, char *target, char *address, size_t address_len, char *port, size_t port_len) {
	struct client_info *cursor = clients;
	while (cursor != NULL) {
		if (strcmp(cursor->nickname, target) == 0) {
			getnameinfo((struct sockaddr *)&(cursor->address), sizeof(cursor->address), address, address_len, port, port_len, NI_NUMERICHOST | NI_NUMERICSERV);
		}
		cursor = cursor->next;
	}
}

// return connection time of the targeted nickname; otherwise 0
time_t get_time(struct client_info *clients, char *target) {
	struct client_info *cursor = clients;
	while (cursor != NULL) {
		if (strcmp(cursor->nickname, target) == 0) {
			return cursor->connection_time;
		}
		cursor = cursor->next;
	}
	return 0;
}

// Add the nicknames of clients into list
void nickname_list(struct client_info *clients, char *list, size_t max_size) {
	struct client_info *cursor = clients;
	while (cursor != NULL) {
		if (strcmp(cursor->nickname, "") != 0) { // To ignore the clients with an empty nickname
			if ((strlen(list) + 2 + strlen(cursor->nickname) + 1) <= max_size) {  // +3 for the extra characters added around each nickname
				strcat(list, "- ");
				strcat(list, cursor->nickname);
				strcat(list, "\n");
			}
		}
		cursor = cursor->next;
	}
}


// Copy the nickname of the client, having this fd, into dest
void get_nickname(struct client_info *clients, int fd, char *dest) {
	struct client_info *cursor = clients;
	while (cursor != NULL) {
		if (cursor->fd == fd) {
			strcpy(dest, cursor->nickname);
			return;
		}
		cursor = cursor->next;
	}
	dest[0] = '\0'; // If fd not found, dest is an empty chain
}

// Return 1 if the nickname of the client in question is empty; 0 otherwise
int empty_nickname(struct client_info *clients, int fd) {
	struct client_info *cursor = clients;
	while (cursor != NULL) {
		if (cursor->fd == fd) {
			if (strcmp(cursor->nickname, "") == 0) {
				return 1;
			}
			return 0;
		}
		cursor = cursor->next;
	}
	return 0;
}


// Return 1 if modifying the nickname is possible; 0 otherwise
int set_nickname(struct client_info *clients, int fd, char *new_nick) {
	struct client_info *cursor = clients;

	if (strlen(new_nick) >= NICK_LEN) {
		fprintf(stderr, "Invalid nickname\n");
		return 0;
	}
	while (cursor != NULL) {
		if (cursor->fd == fd) {
			strcpy(cursor->nickname, new_nick);
			return 1;
		}
		cursor = cursor->next;
	}
	return 0;
}


// Return 1 if nickname is already taken; 0 otherwise
int nickname_exists(struct client_info *clients, int fd, char *nick) {
	struct client_info *cursor = clients;

	while (cursor != NULL) {
		if (cursor->fd != fd && strcmp(cursor->nickname, nick) == 0) {
			return 1;
		}
		cursor = cursor->next;
	}
	return 0;
}


int client_list_add(struct client_info **clients, int fd, const struct sockaddr_storage *address) {
	struct client_info *client = malloc(sizeof(*client)); // Même chose que écrire malloc(sizeof(struct client_info));
	// Par contre, ce serait faux d'écrire malloc(sizeof(client)) sans l'étoile, car client est un pointeur; avec l'étoile, on déréférence donc ok.
	if (client == NULL) {
		return -1;
	}
	client->fd = fd;
	memset(client->nickname, 0, sizeof(client->nickname));
	client->address = *address;
	// Mettre le nouveau client au début de la liste chaînée
	client->connection_time = time(NULL);
	client->next = *clients;
	*clients = client;
	return 0;
}

void client_list_remove(struct client_info **clients, int fd) {
	struct client_info **cursor = clients;

	while (*cursor != NULL) {
		if ((*cursor)->fd == fd) {
			struct client_info *removed = *cursor;
			*cursor = removed->next;
			free(removed);
			return;
		}
		cursor = &((*cursor)->next);
	}
}

void client_list_destroy(struct client_info **clients) {
	while (*clients != NULL) {
		struct client_info *removed = *clients;
		*clients = removed->next;
		free(removed);
	}
}
