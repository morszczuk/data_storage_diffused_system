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

#include "sm_slaves.h"
//#include "structures.h"
//#include "constants.h"

/*//////////////////////////////////////////////
Reads from file that contains actual slaves info
and counts lines - that indicates number of slaves
//////////////////////////////////////////////*/
int get_number_of_slaves(char* filename) {
	int lines = 0;
	FILE* fp;
	int ch;

	fp = fopen(filename, "r");
	if (fp == NULL)
		ERR("fopen");
	while(!feof(fp))
	{
	  ch = fgetc(fp);
	  if(ch == '\n')
	  {
	    lines++;
	  }
	}
	fclose(fp);
	return lines;
}

/*************************
NIE POWINNO TEGO TU BYC!!!!!!!!!!!!!!!!!
*///////////////////////////////


/*//////////////////////////////////////////////
Reads from file that contains actual slaves info, gets info
of every slave and adds them to slaves list
//////////////////////////////////////////////*/
void read_slaves_from_file(char* file, struct node* slaves, int part_size) {
	FILE* fp;
	char* line;
	size_t len = 0;
	struct slave_node* slave_node = 0;
	int read = 0;
	char* addr;

	line = malloc(BUFF_SIZE);

	fp = fopen("server_slaves_list", "r");
	if (fp == NULL)
        ERR("fopen");

	while ((read = getline(&line, &len, fp)) != -1) {
		slave_node = malloc(sizeof(struct slave_node));
		slave_node -> slave_id = atoi(strtok(line," "));
		addr = strtok(NULL, " ");
		slave_node -> addr = malloc(strlen(addr)+1);
		sprintf(slave_node -> addr, "%s", addr);
		slave_node -> port = atoi(strtok (NULL, " "));
		slave_node -> part_size = part_size;
		slave_node -> active = 0;
		add_new_end(slaves, (void*)slave_node);
		add_new_slave_status();

		/*
		printf("CHECK 4\n");
		if(slaves -> slave_id == -1) {
			slave_node = slaves;
			//printf("malloc++\n");
			slave_node -> next = malloc(sizeof(struct slave_node));
			slave_node -> next -> slave_id = -1;
		} else {
			slave_node = slave_node -> next;
			//printf("malloc++\n");
			slave_node -> next = malloc(sizeof(struct slave_node));
			slave_node -> next -> slave_id = -1;
		}

		printf("CHECK 5\n");
		slave_node -> slave_id = atoi(strtok(line," "));
		addr = strtok(NULL, " ");
		slave_node -> addr = malloc(strlen(addr));
		sprintf(slave_node -> addr, "%s", addr);
		printf("SLAVE_NODE ADDR: %s\n", slave_node -> addr);
		slave_node -> port = atoi(strtok (NULL, " "));
		slave_node -> part_size = part_size;
		*/
    }

	fclose(fp);
	free(line);
}

/*//////////////////////////////////////////////
Thread function that connects to specific slave
//////////////////////////////////////////////*/
void* slave_connect(void* args) {
	struct slave_node* slave;
	struct sockaddr_in addr;
	int sock;
	char mess[23];

	slave = (struct slave_node*)args;

	printf("Addres: %s, port: %d", slave -> addr, slave -> port);

	addr = make_address(slave -> addr, slave -> port);
	sock = make_socket(PF_INET, SOCK_STREAM);
	printf("[ID: %d][%s %d] Czekam na połączenie.\n", slave-> slave_id, slave -> addr, slave -> port);
	while( TEMP_FAILURE_RETRY(connect(sock,(struct sockaddr*) &addr,sizeof(struct sockaddr_in))) != 0) {
		sleep(1);
		//ERR("connect");
	}

	slave -> sock = sock;
	slave -> active = 1;

	sprintf(mess, "%10d+%10d", slave -> slave_id, slave -> part_size);
	send_message(sock, "R", mess);

	printf("[ID: %d][%s %d] Połączono.\n", slave -> slave_id, slave -> addr, slave -> port);

	return NULL;
}

/*//////////////////////////////////////////////
Creates thread to connect for every slave that exists.
When it is done, sets global variable and makes system
ready to work.
//////////////////////////////////////////////*/
void* slaves_establish_connection_job(void* args) {
	struct connection_arg* arg;
	arg = (struct connection_arg*)args;
	struct node* node;
	struct slave_node* slave;
	read_slaves_from_file(arg -> file, arg -> slaves_list, arg -> part_size);
	node = arg -> slaves_list -> next;

	while(node -> id != -2) {
		slave = (struct slave_node*)(node -> data);
		printf("CHECK 8\n");
		printf("[SLAVE %d] ADDRES: %s PORT: %d\n", slave -> slave_id, slave -> addr, slave -> port);
		if((pthread_create(&(slave -> connection_tid), NULL, slave_connect, (void*)slave)) < 0)
			ERR("pthread create:");
		node = node -> next;
		printf("CHECK 9\n");
	}

	node = arg -> slaves_list -> next;
	while(node -> id != -2) {
		slave = (struct slave_node*)(node -> data);
		pthread_join(slave -> connection_tid, NULL);
		node = node -> next;
	}
	printf("CHECK 10\n");
	printf("System gotowy do działania!\n");
	pthread_mutex_lock(arg -> lock);
	system_ready = 1;
	pthread_mutex_unlock(arg -> lock);


	return NULL;
}

/*//////////////////////////////////////////////
Creates thread that takes care of connecting to slaves
//////////////////////////////////////////////*/
int slaves_establish_connection(int fd, char* filename, struct node* slaves_list, struct connection_arg* arg, int part_size, pthread_mutex_t* lock) {
	pthread_t tid = 0;

	arg -> file = filename;
	arg -> sock = fd;
	arg -> slaves_list = slaves_list;
	arg -> part_size = part_size;
    arg -> lock = lock;

	printf("CHECK 1\n");
	if((pthread_create(&(tid), NULL, slaves_establish_connection_job, (void*)arg)) < 0)
		ERR("pthread create:");

	return tid;
}
