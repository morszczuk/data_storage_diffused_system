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

#include "files_management.h"

/*//////////////////////////////////////////////
Writes new file to a file that contains files infos.
//////////////////////////////////////////////*/
void write_new_file_to_file(struct file_info* new_file) {
	FILE* fp;

	fp = fopen("files", "ab+");
	printf("write new file to file 1\n");
	fprintf(fp, "%d %s %d %d\n", new_file -> file_id, new_file -> filename, new_file -> file_size, new_file -> num_of_parts);
	printf("write new file to file 2\n");
	fclose(fp);
}

/*//////////////////////////////////////////////
Reads from file that contains files info and adds them
to the file list
//////////////////////////////////////////////*/
void read_file_infos_from_file(struct file_info* files_list) {
	FILE* fp;
	char* line, *file_id, *file_size, *num_of_parts;
	size_t len = 0;
	int read;
	struct file_info* new_file;

	//printf("malloc++\n");
	line = malloc(BUFF_SIZE);

	fp = fopen("tmp/files", "ab+");
	if (fp == NULL)
		ERR("fopen");
		printf("CZYTAM PLIKI Z PLIKUUU\n");
	while ((read = getline(&line, &len, fp)) != -1) {
		printf("Plik wyryty!!!\n");
		new_file = malloc(sizeof(struct file_info));
		file_id = strtok(line, " ");
		new_file -> file_id = atoi(file_id);
		new_file -> filename = strtok(NULL, " ");
		file_size = strtok(NULL, " ");
		num_of_parts = strtok(NULL, " ");
		new_file -> file_size = atoi(file_size);
		new_file -> num_of_parts = atoi(num_of_parts);
		add_new_file(new_file, files_list);
	}

	free(line);
	fclose(fp);
}

/*//////////////////////////////////////////////
Adds new file info to the list
//////////////////////////////////////////////*/
void add_new_file(struct file_info* new_file, struct file_info* files_list) {
	struct file_info* p, *pp;
	p = files_list;
	pp = files_list -> next;

	printf("Adding new file\n");
	while(pp -> file_id != -2) {
		p = p -> next;
		pp = pp -> next;
	}

	p-> next = new_file;
	new_file -> next = pp;
	if(new_file -> file_id == -1 ) {
		printf("[POPRZEDNIE ID: %d]\n\n\n\n\n\n", p -> file_id);
		new_file -> file_id = (p -> file_id) + 1;
	}
}

/*//////////////////////////////////////////////
Reads file info from message sent by upload client
//////////////////////////////////////////////*/
void read_file_info_from_mess(struct file_info* file, char* buff) {
	char* slave_id;
	char* file_size;
	char* num_of_parts;
	char* filename;

	file -> filename = strtok(buff, "+");
	file_size = strtok(NULL, "+");
	num_of_parts = strtok(NULL, "+");

	file -> file_size = atoi(file_size);
	file -> num_of_parts = atoi(num_of_parts);

}

struct part_info* read_part_info_from_mess(char* buff) {
	char* part_id;
	char* file_id;
	char* data;
	struct part_info* part_info;
	part_info = malloc(sizeof(struct part_info));

	file_id = strtok(buff, "+");
	part_id = strtok(NULL, "+");
	part_info -> data = strtok(NULL, "+");

	part_info -> file_id = atoi(file_id);
	part_info -> part_id = atoi(part_id);

	return part_info;
}
