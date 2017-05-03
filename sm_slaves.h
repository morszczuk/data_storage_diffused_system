#ifndef SM_SLAVES_H
#define SM_SLAVES_H

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
#include "connection.h"

int get_number_of_slaves(char* filename);
void read_slaves_from_file(char* file, struct node* slaves, int part_size);
void* slave_connect(void* args);
void* slaves_establish_connection_job(void* args);
int slaves_establish_connection(int fd, char* filename, struct node* slaves_list,
	struct connection_arg* arg, int part_size, pthread_mutex_t* lock);


#endif
