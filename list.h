#ifndef LIST_H
#define LIST_H

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

#include "structures.h"
#include "constants.h"

struct node* initialize_list();

void add_new_beg(struct node* list, void* new_elem);

int add_new_end(struct node* list, void* new_elem);

void free_list(struct node* list);

struct node* find_elem(struct node* list, int _id);

int count_elems(struct node* list);

int count_active_slaves(struct node* list);

#endif
