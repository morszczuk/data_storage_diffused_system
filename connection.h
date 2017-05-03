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
#include "list.h"
#include "files_management.h"
#include "connection_utils.h"

int send_message(int fd, char* mess_type, char* mess);
void* read_message_sm(int fd);

#endif /* CONNECTION_H*/
