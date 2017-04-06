/*
 * File:  new_server_main.c
 * --------------------
 * This file defines main server for diffused data storage system
 * [project Djk, http://www.mini.pw.edu.pl/~marcinbo/strona/zadania/unixprojekt2016.html]
 * Main server has certain functonalities:
 *
 *
 *
 *
 *  n: number of terms in the series to sum
 *
 *  returns: the approximate value of pi obtained by suming the first n terms
 *           in the above series
 *           returns zero on error (if n is non-positive)
 */



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
#include "connection.h"
#include "utils.h"
#include "sm_slaves.h"
#include "files_management.h"
#include "sm_upload.h"
#include "statuses_list.h"

void* downloader(void* args) {
	int fd, new_fd;
	char header_buff[100];
	int a;

	fd = bind_inet_socket(1000, SOCK_STREAM);

	while((new_fd = add_new_client(fd)) <= 0)
		a = 5;

	read(new_fd, header_buff, 10);
	printf("HTML PROTOCOL REQUEST!!!!: %s", header_buff);

	return NULL;

}



int main(int argc, char** argv) {
	pthread_t establish_connection_tid, uploader_tid, administrative_tid, downloader_tid;
	struct connection_arg* arg = 0;
	struct slave_node* slaves_list = 0;
	struct slave_node* node = 0;
	struct file_info* files_list = 0;
  int fd, new_flags;
  int system_reliability = 0;
  int slaves_count = 0;
	int part_size = 9;
	pthread_mutex_t lock;
	struct node* slaves;

  srand(time(NULL));

  if(argc<3) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	//default 0
  if(argc == 4) system_reliability = atoi(argv[3]);
	//default 9
	if(argc== 5) part_size = atoi(argv[4]);

	//ignoring(SIG_IGN) signal SIGPIPE that is sent to a process if it tried to write to a socket that had been shutdown for writing or isn't connected (anymore)
  if(sethandler(SIG_IGN,SIGPIPE)) ERR("Seting SIGPIPE:");
	//processing SIGINT sygnal - in our case, the main loop will be ended
	if(sethandler(sigint_handler,SIGINT)) ERR("Seting SIGINT:");
	//Mutexes initialization
  if (pthread_mutex_init(&lock, NULL) != 0) ERR("Mutex init:");
	if (pthread_mutex_init(&lock_lock, NULL) != 0) ERR("Mutex init:");

	//checking number of slaves that are going to be connected to the system from the file
  if((slaves_count = get_number_of_slaves(argv[2])) <= system_reliability) {
		printf("Zbyt mało slave'ów aby system działał!\n");
		exit(EXIT_FAILURE);
	}

	//if((pthread_create(&downloader_tid, NULL, downloader, (void*)NULL)) < 0)
	//	ERR("pthread_create:");

  fd = bind_inet_socket(atoi(argv[1]),SOCK_STREAM);

  new_flags = fcntl(fd, F_GETFL) | O_NONBLOCK;
  fcntl(fd, F_SETFL, new_flags);

	slaves = initialize_list();
	statuses_list = initialize_list();
	/*
	slaves_list = malloc(sizeof(struct slave_node));
	printf("malloc++\n");
	slaves_list -> slave_id = -1;
	*/
	arg = malloc(sizeof(struct connection_arg));
	printf("malloc++\n");

	//Reading information about files currently stored in a server
	files_list = malloc(sizeof(struct file_info));
	files_list -> file_id = -1;
	files_list -> next = malloc(sizeof(struct file_info));
	files_list -> next -> file_id = -2;
	read_file_infos_from_file(files_list);

	//Creates new thread, that creates new threads responsible for connecting with certain slaves
	establish_connection_tid = slaves_establish_connection(fd, argv[2], slaves, arg, part_size, &lock);
	//
	uploader_tid = handle_upload_clients(fd, part_size, files_list, &lock, slaves, system_reliability);
	//administrative_tid = handle_administrative_clients()

	pthread_join(establish_connection_tid, NULL);
	pthread_join(uploader_tid, NULL);
	//pthread_join(downloader_tid, NULL);

	/*
	while(slaves_list -> slave_id != -1 ) {
		node = slaves_list;
		slaves_list = slaves_list -> next;
		if(TEMP_FAILURE_RETRY(close(node -> sock))<0) ERR("close");
		free(node);
	}
	*/

	if(TEMP_FAILURE_RETRY(close(fd))<0) ERR("close");

	//free(slaves_list);
	free_list(slaves);
	free(arg);

	exit(EXIT_SUCCESS);
}
