#ifndef JALON1_COMMON_H
#define JALON1_COMMON_H

#include <stddef.h>
#define MAX_PAYLOAD_SIZE 4096

struct message;

void die(int val, char *msg);
int read_from_socket(int fd, void *buf, size_t msg_size);
int write_in_socket(int fd, void *buf, size_t msg_size);
int send_structure_and_payload(int fd, struct message *message, char *payload);
int receive_structure_and_payload(int fd, struct message *message, char *payload, int max_size);
int valid_nickname(char *pseudo, size_t max_size);

#endif
