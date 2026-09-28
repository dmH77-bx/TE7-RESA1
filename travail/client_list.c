#include "client_list.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define NICK_LEN 128

struct client_info {
	int fd;
	char nickname[NICK_LEN];
	struct sockaddr_storage address;
	struct client_info *next;
};

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
