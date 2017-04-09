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

void read_html_request(int fd, struct node* slaves){
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
		serve_file(fd, file_id, slaves);
	}

	free(request);
	close(fd);
}

void serve_file(int fd, int file_id, struct node* slaves){
	struct node* files_list;
	struct file_info* file;
	struct node* tids_list;
	int i = 0;
	pthread_t* tid;
	struct part_info* part_info;
	struct get_file_part_arg* file_part_arg;
	struct node* node;
	tids_list = initialize_list();
	void* data = 0;
	char* file_data;
	char* response;
	char* response_body;

	file_data = malloc(1024);
	response = malloc(1024);
response_body = malloc(1024);

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
		file_part_arg = malloc(sizeof(struct get_file_part_arg));
		file_part_arg -> part = part_info;
		file_part_arg -> slaves = slaves;
		sleep(1);
		pthread_create(tid, NULL, get_file_part, (void*)file_part_arg);
		add_new_end(tids_list, (void*)(tid));
	}

	node = tids_list -> next;
	while(node -> id != -2) {
		pthread_join(*((pthread_t*)(node -> data)), &data);
		if(data == NULL) {

				response_body = "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\"><html><head><title>Błąd!</title></head><body><h1>Bad Request</h1><p>Liczba wyłączonych slave\'ów przekroczyła poziom niezawodności tego pliku.<p></body></html>";
			sprintf(response,
				"HTTP/1.1 400 Bad Request\r\n"
				"Content-Length: %d\r\n"
				"Content-Type: text/html; charset=utf-8\r\n"
				"Connection: Closed\r\n"
				"\r\n"
				"%s",
				strlen(response_body), response_body);

			bulk_write(fd, response, strlen(response));
			return;
		}

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
	struct get_file_part_arg* arg;
	FILE* fp;
	char filename[100];
	int read;
	char* line;
	size_t len;
	int slave_id = -1;
	struct node* slave;

	line = malloc(100);
	arg = (struct get_file_part_arg*)args;
	part_info = arg -> part;

	printf("OTRZYMANE DO WĄTKU FILE_ID: %d PART_ID: %d\n", part_info -> file_id, part_info -> part_id);

	sprintf(filename, "tmp/file_info/%d/%d/slaves", part_info -> file_id, part_info -> part_id);

	fp = fopen(filename, "r");
	if (fp == NULL)
        ERR("fopen");

	//printf("BĘDĘ UDERZAŁ PO [PART %d] DO SLAVE'a %d\n", part_info -> part_id, slave_id);
	//MIRON TUTAJ MUSISZ ZADZIAŁAĆ CUDA

	//Czytam info plik o części. Zawiera on informację,które slave'y go posiadają. Sprawdzam, czy którykolwiek slave jest dostępny.
	printf("GET FILE PART 0\n\n");
	while ((read = getline(&line, &len, fp)) != -1) {
		slave_id = atoi(line);
		slave = find_active_elem(arg -> slaves, slave_id);
		if(slave -> id != -2)
			break;
	}

	fclose(fp);

	if(slave -> id == -2) {
		printf("PRZEKROCZONO LICZBĘ WYŁĄCZONYCH SLAVE'ÓW DLA TEGO PLIKU!");
		////TUTAJ TRZEBA ZROBIĆ ODPOWIEDŹ W HTML'U BŁĘDNEGO REQUEST'U!!!
		return NULL;
	}

	printf("ZNALAZŁEM SLAVE ELEMENT O ID: %d\n\n", slave -> id);

	if(get_part_from_slave((struct slave_node*)(slave -> data), part_info) == 1) {
		printf("JEST OK\n\n\n");
		return (void*)part_info;
		///TUTAJ UDAŁO NAM SIĘ DOSTAĆ CZĘŚĆ OD SLAVE"A
	} else {
		printf("Nie udało się pobrać part'a od slave'a");
	}
/*
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
	//return NULL;*/
}

int get_part_from_slave(struct slave_node* slave, struct part_info* part) {
	char* mess;
	struct message* response;

	response = malloc(sizeof(struct message));

	mess = malloc(22);
	sprintf(mess, "%10d+%10d", part -> file_id, part -> part_id);
	send_message(slave -> sock, "D", mess);

	free(mess);

	read_mess(slave -> sock, response);

	printf("I GOT RESPONSEE!!!! : %s \n\n\n\n", response -> message);
	part -> data = response -> message;
	return 1;
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
	strcat(response,"<!DOCTYPE html><html><head><link rel=\"shortcut icon\" href=\"data:image/x-icon;,\" type=\"image/x-icon\"></head><body><ol>");
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
	strcat(response, "</ol></body>");
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
