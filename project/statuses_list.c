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

#include "statuses_list.h"

struct node* statuses_list;

struct node* get_unread_statuses() {
    struct node* p, *pp, *node;
    struct node* return_list = initialize_list();

    p = statuses_list;
    pp = p -> next;

    while(pp -> id != -2){
        node = pp;
        add_new_end(return_list, (void*)(node->data));
        p -> next = pp -> next;
        pp = pp -> next;
        free(node -> data);
        free(node);
    }

    return return_list;
}

void add_new_client_status(){
    char* status = "[Dodano nowego klienta]";

    add_new_end(statuses_list, (void*)status);
}

void add_new_slave_status(){
    char* status = "[Dodano nowy slave]";

    add_new_end(statuses_list, (void*)status);
}

void add_new_file_status(){
    char* status = "[Wgrano nowy plik]";

    add_new_end(statuses_list, (void*)status);
}

void add_file_downloaded_status(){
    char* status = "[Pobrano plik]";

    add_new_end(statuses_list, (void*)status);
}
