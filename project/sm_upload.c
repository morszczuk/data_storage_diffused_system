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

#include "sm_upload.h"

/*//////////////////////////////////////////////
Main upload loop, that waits for upload clients, when it happens creates new thread
that handles upload proccess for a new upload client.
//////////////////////////////////////////////*/
int handle_upload_clients(int fd, int part_size, struct file_info* files_list,
	pthread_mutex_t* lock, struct node* slaves, int system_reliability) {
	int new_fd;
	struct uploader_thread* upload_threads, *up_thread;

	upload_threads = malloc(sizeof(struct uploader_thread));
	upload_threads -> fd = -1;
	up_thread = upload_threads;

	while(sm_do_work) {
		if((new_fd = add_new_client(fd)) > 0) {

			if(up_thread -> fd != -1 )
				up_thread = up_thread -> next;

			up_thread -> next = malloc(sizeof(struct uploader_thread));
			up_thread -> next -> fd = -1;

			up_thread -> fd = fd;
			up_thread -> new_fd = new_fd;
			up_thread -> part_size = part_size;
			up_thread -> files_list = files_list;
			up_thread ->  lock = lock;
			up_thread -> slaves = slaves;
			up_thread -> system_reliability = system_reliability;

			if((pthread_create(&(up_thread -> tid), NULL, manage_upload, (void*)up_thread)) < 0)
				ERR("pthread_create:");

		}
		sleep(1);
	}

	up_thread = upload_threads;
	while(up_thread -> fd != -1) {
		pthread_join(up_thread -> tid, NULL);
		up_thread = up_thread -> next;
	}

	while(upload_threads -> fd != -1) {
		up_thread = upload_threads;
		upload_threads = upload_threads -> next;
		free(up_thread);
	}

	free(upload_threads);
	return 0;
}

void* handle_part(void* args){
	struct handle_part_arg *arg;
	struct part_info* part;
	struct node* slave;
	int slaves_count = 0;
	int diff = 0;
	int i = 0;
	struct slave_node* slave_node;
	char* mess;
	srand(time(NULL));
	printf("Handle part 1\n");

	pthread_mutex_lock(&lock_lock);
	arg = (struct handle_part_arg*)args;
	part = arg -> part;
	printf("Handle part 2\n");
	printf("Otrzymane dane: [FILE_ID: %d] [PART_ID: %d] [PART: %s]\n", part -> file_id, part -> part_id, part -> data);
	slaves_count = count_elems(arg -> slaves);
	printf("Handle part 3\n\n\n");
	diff = rand()%(slaves_count - (arg -> system_reliability ) - 1);
	printf("Handle part 3.0.1 [DIFF: %d]\n\n\n", diff);
	slave = arg -> slaves -> next;
	printf("Handle part 3.0,2\n\n\n");
	while(i < diff) {
		printf("Handle part 3.0.3\n\n\n");
		slave = slave -> next;
		printf("Handle part 3.0.4\n\n\n");
		printf("Handle part 3.1 [NODE ID: %d]\n", slave -> id);
		printf("Handle part 3.0.5\n\n\n");
		i++;
	}
	printf("Handle part 3.2\n\n\n");
	printf("Handle part 4\n");
	mess = malloc(23 + strlen(part -> data));
	printf("Handle part 4.0.1\n");
	sprintf(mess, "%10d+%10d+%s", part -> file_id, part -> part_id, part -> data);
	printf("Handle part 4.0.2\n");
	for(i = 0; i < (arg -> system_reliability) + 1; i++) {
		printf("Handle part 4.1\n");

		printf("Handle part 4.2\n");
		slave_node = (struct slave_node*)(slave -> data);
		printf("Handle part 4.3\n");
		send_message(slave_node-> sock, "P", mess);
		slave = slave -> next;
	}
	printf("Handle part 5\n");
	pthread_mutex_unlock(&lock_lock);
	free(mess);

}

void distribute_file(int fd, struct file_info* new_file, struct node* slaves, int system_reliability) {
	pthread_t tids[new_file -> num_of_parts];
	struct handle_part_arg args[new_file -> num_of_parts];
	int i = 0;

	while(i < new_file -> num_of_parts) {
		args[i].part = (struct part_info*)read_message_sm(fd);
		args[i].slaves = slaves;
		args[i].system_reliability = system_reliability;
		if((pthread_create(&(tids[i]), NULL, handle_part, (void*)&(args[i]))) < 0)
			ERR("pthread_create:");
		i++;
	}

	for(i = 0; i < new_file -> num_of_parts; i++) {
		pthread_join(tids[i], NULL);
	}

}

/*//////////////////////////////////////////////
Thread function, that handles upload client and
uploading proccess of a new file
//////////////////////////////////////////////*/
void* manage_upload(void* args) {
	struct uploader_thread* uploader;
	struct file_info* new_file;
	printf("Manage upload 1\n");
	uploader = (struct uploader_thread*) args;
	printf("Manage upload 2\n");
	wait_for_system_readiness(uploader -> new_fd, uploader -> lock);
	printf("Manage upload 3\n");
	if(sm_do_work)
		send_size_mess(uploader -> new_fd, uploader -> part_size);

	sleep(1);
	printf("Manage upload 4\n");
	new_file = get_file_info(uploader -> new_fd, uploader -> files_list);
	printf("Manage upload 5\n");
	distribute_file(uploader -> new_fd, new_file, uploader -> slaves, uploader -> system_reliability);
	printf("Manage upload 6\n");
	return NULL;
}

/*//////////////////////////////////////////////
Loop that waits for all of the slaves to be connected
//////////////////////////////////////////////*/
void wait_for_system_readiness(int fd, pthread_mutex_t* lock) {
	int is_system_ready = 0;

	pthread_mutex_lock(lock);
	is_system_ready = system_ready;
	pthread_mutex_unlock(lock);

	while(!is_system_ready && sm_do_work) {
		send_size_mess(fd, 0);
		sleep(1);
		pthread_mutex_lock(lock);
		is_system_ready = system_ready;
		pthread_mutex_unlock(lock);
	}
}

/*//////////////////////////////////////////////
Sends size of the part to the upload client
//////////////////////////////////////////////*/
void send_size_mess(int fd, int part_size) {
	char* mess;
	mess = malloc(BUFF_SIZE);

	sprintf(mess, "%10d", part_size);

	if(send_message(fd, "R", mess) < 0)
		ERR("bulk_write");

	free(mess);
}

/*//////////////////////////////////////////////
Reads file info sent by upload client before uploading file,
adds file info to the list and to file
//////////////////////////////////////////////*/
struct file_info* get_file_info(int fd, struct file_info* files_list) {
	struct file_info* new_file;
	char* file_id;
	file_id = malloc(BUFF_SIZE+1);
	printf("Get file info 1\n");
	new_file = (struct file_info*)read_message_sm(fd);
	new_file -> file_id = -1;
	printf("Get file info 2\n");
	//printf("Otrzymałem file info [FILENAME: %s] [FILESIZE: %d] [NUM OF PARTS: %d]\n", new_file -> filename, new_file -> file_size, new_file -> num_of_parts);
	printf("Get file info 2.1\n");
	add_new_file(new_file, files_list);
printf("Get file info 2.2\n");
	write_new_file_to_file(new_file);
	printf("Get file info 3\n");
	sprintf(file_id, "%10d", new_file -> file_id);
	file_id[10] = '\0';
	printf("Get file info 4\n");
	printf("Przygotowane id: %s\n", file_id );
	send_message(fd, "F", file_id);
	printf("Get file info 5\n");
	free(file_id);

	return new_file;
}