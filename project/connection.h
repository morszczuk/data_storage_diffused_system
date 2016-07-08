#ifndef CONNECTION_H
#define CONNECTION_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <fcntl.h>
#include <pthread.h>

#include "utils.h"
#include "structures.h"
#include "files_management.h"

struct sockaddr_in make_address(char *address, uint16_t port);
int make_socket(int domain, int type);
int bind_inet_socket(uint16_t port, int type);
int add_new_client(int sfd);
ssize_t bulk_write(int fd, char *buf, size_t count);
ssize_t bulk_read(int fd, char *buf, size_t count);
int send_message(int fd, char* mess_type, char* mess);
void* read_message_sm(int fd);

#endif /* CONNECTION_H*/
