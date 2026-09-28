#ifndef JALON1_CLIENT_LIST_H
#define JALON1_CLIENT_LIST_H

#include <netinet/in.h>

struct client_info;

int change_nickname(struct client_info *clients, int fd, char *new_nick);
int nickname_exists(struct client_info *clients, int fd, char *nick);
int client_list_add(struct client_info **clients, int fd, const struct sockaddr_storage *address);
void client_list_remove(struct client_info **clients, int fd);
void client_list_destroy(struct client_info **clients);

#endif
