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

#include "html_client.h"

void read_html_request(int fd){
	char* request;
	char* dir;
	int c = 0;
	ssize_t len;
	int file_id = -1;

	request = malloc(100);
	c = read(fd, request, 100);
	printf("HTML REQUEST: %s\n", request);

	dir = strtok(request, " ");
	if(strcmp(dir, "/") == 0) {
		printf("UDERZAMY PO GŁÓWNĄ\n");
		serve_files_page(fd);
	}
	else {
		file_id = atoi(strtok(dir, "/"));
		printf("UDERZAMY PO KONKRETNY PLIK NUMER: %d\n", file_id);
		serve_file(fd, file_id);
	}

	free(request);
	close(fd);
}

void serve_file(int fd, int file_id){
	struct node* files_list;
	struct file_info* file;
	struct node* tids_list, *slaves;
	int i = 0;
	pthread_t* tid;
	struct part_info* part_info;
	struct node* node;
	tids_list = initialize_list();
	void* data = 0;
	char* file_data;
	char* response;

	file_data = malloc(1024);
	response = malloc(1024);

	files_list = prepare_list_of_files();
	files_list = files_list -> next;
	while(files_list -> id != -2){
		file = (struct file_info*)(files_list -> data);
		if( file -> file_id == file_id)
		{
			printf("ZNALEZIONO PLIK! ID: %d NUM OF PARTS: %d\n", file -> file_id, file -> num_of_parts);
			break;
		}
		files_list = files_list -> next;
	}

	if(files_list -> id == -2) return;

	for(i = 0; i < file -> num_of_parts; i++) {
		tid = malloc(sizeof(pthread_t));
		part_info = malloc(sizeof(struct part_info));
		part_info -> file_id = file_id;
		part_info -> part_id = i;
		pthread_create(tid, NULL, get_file_part, (void*)part_info);
		add_new_end(tids_list, (void*)(tid));
	}

	node = tids_list -> next;
	while(node -> id != -2) {
		pthread_join(*((pthread_t*)(node -> data)), &data);
		part_info = (struct part_info*)data;
		strcat(file_data, part_info -> data);
		node = node -> next;
	}

	printf("OTRZYMANY PLIK: %s\n", file_data);

	sprintf(response,
		"HTTP/1.1 200 OK\r\n"
		"Content-Disposition: attachment; filename=\"%s\"\r\n"
		"Content-Type: text/plain\r\n"
		"Connection: close\r\n"
		"Content-Length: %d\r\n"
		"\r\n"
		"%s"
		,file -> filename, strlen(file_data), file_data);

	bulk_write(fd, response, strlen(response));

	add_file_downloaded_status();
}

void* get_file_part(void* args) {
	struct part_info* part_info;
	FILE* fp;
	char filename[100];
	int read;
	char* line;
	size_t len;
	int slave_id;

	line = malloc(100);
	part_info = (struct part_info*)args;

	printf("OTRZYMANE DO WĄTKU FILE_ID: %d PART_ID: %d\n", part_info -> file_id, part_info -> part_id);

	sprintf(filename, "tmp/file_info/%d/%d/slaves", part_info -> file_id, part_info -> part_id);

	fp = fopen(filename, "r");
	if (fp == NULL)
        ERR("fopen");

	read = getline(&line, &len, fp);
	slave_id = atoi(line);
	//printf("BĘDĘ UDERZAŁ PO [PART %d] DO SLAVE'a %d\n", part_info -> part_id, slave_id);

	fclose(fp);

	sprintf(filename, "tmp/parts/%d/%d/%d", slave_id, part_info -> file_id, part_info -> part_id);
	fp = fopen(filename, "r");
	if (fp == NULL)
        ERR("fopen");

	read = getline(&line, &len, fp);
	part_info -> data = line;
	printf("PRZECZYTANY W WĄTKU PART: %s\n", part_info -> data);

	fclose(fp);
	free(line);
	return (void*)part_info;
	//return NULL;
}

void serve_files_page(int fd) {
	char* reply;
	char* html_files_response;

	reply = malloc(2048);
	html_files_response = files_response();
	sprintf(reply,
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/html\r\n"
		"Connection: close\r\n"
		"Content-Length: %d\r\n"
		"\r\n"
		"%s"
		,strlen(html_files_response), html_files_response);

	bulk_write(fd, reply, strlen(reply));

	free(reply);
	free(html_files_response);
}

char* files_response() {
	struct node* files_list;
	char* response;
	char buff[1024];
	struct node* file_node;
	struct file_info* file;

	response = malloc(2048);

	printf("FILES RESPONSE 1\n");
	files_list = prepare_list_of_files();
	printf("FILES RESPONSE 2\n");
	strcat(response,"<!DOCTYPE html><html><head><link rel=\"shortcut icon\" href=\"data:image/x-icon;,\" type=\"image/x-icon\"></head><body><ul>");
	printf("FILES RESPONSE 3\n");
	file_node = files_list -> next;
	while(file_node -> id != -2){
		file = (struct file_info*)(file_node -> data);
		printf("WPISUJE DO PLIKU FILE_ID %d FILENAME %s\n", file -> file_id, file -> filename);
		strcat(response, "<li>");
		sprintf(buff, "<a href=\"/%d\">%s</a>", file -> file_id, file -> filename);
		strcat(response, buff);
		strcat(response, "</li>");
		file_node = file_node -> next;
	}
	printf("FILES RESPONSE 4\n");

	printf("FILES RESPONSE 5\n");
	strcat(response, "</ul></body>");
	printf("FILES RESPONSE 6\n");
	printf("FILES RESPONSE: %s\n", response);
	return response;
}

struct node* prepare_list_of_files() {
	FILE* fp;
	char* line, *file_id, *file_size, *num_of_parts, *filename;
	size_t len = 0;
	int read;
	struct file_info* new_file;
	struct node* files_list = initialize_list();
	struct node* node;

	//printf"malloc++\n")
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
		filename = strtok(NULL, " ");
		new_file -> filename = malloc(strlen(filename));
		strcpy(new_file -> filename, filename);
		printf("FILE READ FROM FILE: %s\n", new_file -> filename);
		file_size = strtok(NULL, " ");
		num_of_parts = strtok(NULL, " ");
		new_file -> file_size = atoi(file_size);
		new_file -> num_of_parts = atoi(num_of_parts);
		add_new_end(files_list, (void*)new_file);
	}

	node = files_list -> next;

	while(node -> id != -2) {
		printf("PRZEGLĄDAM LISTĘ PLIKÓW, FILENAME: %s\n", ((struct file_info*)(node -> data))-> filename);
		node = node -> next;
	}

	free(line);
	fclose(fp);
	return files_list;
}
