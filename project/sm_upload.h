#ifndef SM_UPLOAD_H
#define SM_UPLOAD_H

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
#include "files_management.h"
#include "html_client.h"
#include "read_message.h"

struct client_thread_arg* new_client_thread_arg(int fd, int new_fd,int part_size,struct file_info* files_list,
	pthread_mutex_t* lock, struct node* slaves, int system_reliability, char* type);
int handle_upload_clients(int fd, int part_size, struct file_info* files_list,
    pthread_mutex_t* lock, struct node* slaves, int system_reliability);
void* handle_part(void* args);
void distribute_file(int fd, struct file_info* new_file, struct node* slaves, int system_reliability);
void send_size_mess(int fd, int part_size);
struct file_info* get_file_info(int fd, struct file_info* files_list);
void* manage_client(void* args);
void wait_for_system_readiness(int fd, pthread_mutex_t* lock);

#endif
