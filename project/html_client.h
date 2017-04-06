#ifndef HTML_CLIENT_H
#define HTML_CLIENT_H

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
#include "statuses_list.h"

void read_html_request(int fd);
void serve_files_page(int fd);
char* files_response();
struct node* prepare_list_of_files();
void* get_file_part(void* args);
void serve_file(int fd, int file_id);

#endif
