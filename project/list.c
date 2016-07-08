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

#include "list.h"

struct node* initialize_list(){
    struct node* list;

    list = malloc(sizeof(struct node));
    list -> id = -1;

    list -> next = malloc(sizeof(struct node));
    list -> next -> id = -2;

    return list;
}

void add_new_beg(struct node* list, void* new_elem){
    struct node* p;

    p = malloc(sizeof(struct node));
    p -> data = new_elem;
    p -> id = 0;

    p -> next = list -> next;
    list -> next = p;
}

int add_new_end(struct node* list, void* new_elem) {
    struct node* new_node;
    struct node* p, *pp;

    p = list;
    pp = list -> next;

    new_node = malloc(sizeof(struct node));

    new_node -> data = new_elem;

    printf("Dodajemy. pID: %d | ppID: %d\n", p -> id, pp -> id);
    while(pp -> id != -2){
        printf("Mijam kolejny element\n");
        p = p -> next;
        pp = pp -> next;
    }

    new_node -> id = (p -> id) + 1;
    new_node -> next = pp;
    p -> next = new_node;

    return new_node -> id;
}

void free_list(struct node* list){
    struct node* p, *pp, *elem;

    p = list;
    pp = list -> next;

    while(pp -> id != -2){
        elem = pp;
        pp = pp -> next;
        free(elem -> data);
        free(elem);
    }

    free(pp);
    free(p);
}

int count_elems(struct node* list){
    struct node* p, *pp;
    int i = 0;

    p = list;
    pp = list -> next;

    while(pp -> id != -2){
        p = p -> next;
        pp = pp -> next;
        i++;
    }
    return i;
}
