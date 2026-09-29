#ifndef JALON1_CLIENT_LIST_H
#define JALON1_CLIENT_LIST_H

#include <netinet/in.h>

struct client_info;

void get_address_and_port(struct client_info *clients, char *target, char *address, size_t address_len, char *port, size_t port_len);
time_t get_time(struct client_info *clients, char *target);
void nickname_list(struct client_info *clients, char *list, size_t max_size);
void get_nickname(struct client_info *clients, int fd, char *dest);
int empty_nickname(struct client_info *clients, int fd);
int set_nickname(struct client_info *clients, int fd, char *new_nick);
int nickname_exists(struct client_info *clients, int fd, char *nick);
int client_list_add(struct client_info **clients, int fd, const struct sockaddr_storage *address);
void client_list_remove(struct client_info **clients, int fd);
void client_list_destroy(struct client_info **clients);

#endif
