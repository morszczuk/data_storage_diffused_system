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
#include "structures.h"

int main(int argc, char** argv) {
    struct node* parts_list;
    struct part_info* part;
    int id, list_count = 0;

    part = malloc(sizeof(struct part_info));
    part -> file_id = 0;

    parts_list = initialize_list();
    printf("ID0: %d | ID1: %d\n", parts_list -> id, parts_list -> next -> id);
    printf("Przed dodaniem \n");
    printf("Po dodaniu\n");
    part -> part_id = add_new_end(parts_list, (void*)part);
    //add_new_end(parts_list, (void*)part);
    //add_new_end(parts_list, (void*)part);
    //id = add_new_end(parts_list, (void*)part);

    list_count = count_elems(parts_list);

    printf("[ID:] [LIST COUNT: %d]\n", list_count);

    free_list(parts_list);

    return 0;
}
