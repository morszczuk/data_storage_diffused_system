#ifndef READ_MESSAGE_H
#define READ_MESSAGE_H

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

void read_mess(int fd, struct message* mess);
void read_html_request(int fd);
void read_first_client_mess(int fd, struct message* mess);
char* files_response();

#endif
