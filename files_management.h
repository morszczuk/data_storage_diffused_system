#ifndef FILES_MANAGEMENT_H
#define FILES_MANAGEMENT_H

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

void write_new_file_to_file(struct file_info* new_file);
void read_file_infos_from_file(struct file_info* files_list);
void add_new_file(struct file_info* new_file, struct file_info* files_list);
void read_file_info_from_mess(struct file_info* file, char* buff);
//struct node* prepare_list_of_files();


#endif
