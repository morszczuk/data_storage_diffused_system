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
#include <assert.h>

#include "constants.h"


volatile sig_atomic_t do_work=1 ;

struct file_info {
	int file_id;
	char* filename;
	int file_size;
	int num_of_parts;
};

void usage(char* name);
void sigint_handler(int sig);
int sethandler( void (*f)(int), int sigNo);
int make_socket(void);
struct sockaddr_in make_address(char *address, uint16_t port);
ssize_t bulk_read(int fd, char *buf, size_t count);
ssize_t bulk_write(int fd, char *buf, size_t count);
int connect_socket(char *name, uint16_t port);
void* read_message(int fd);
struct file_info* get_file_info(int fd, char* filename, int file_size);
int send_file_info(int fd, char* filename, int file_size, struct file_info* file_info);
int wait_for_system_readiness(int fd);

//functions definitions
void usage(char * name){
	fprintf(stderr,"USAGE: %s domain port filename\n",name);
}

void sigint_handler(int sig) {
	do_work=0;
}

int sethandler( void (*f)(int), int sigNo) {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if (-1==sigaction(sigNo, &act, NULL))
		return -1;
	return 0;
}

int make_socket(void){
	int sock;
	sock = socket(PF_INET,SOCK_STREAM,0);
	if(sock < 0) ERR("socket");
	return sock;
}

struct sockaddr_in make_address(char *address, uint16_t port){
	struct sockaddr_in addr;
	struct hostent *hostinfo;
	addr.sin_family = AF_INET;
	addr.sin_port = htons (port);
	hostinfo = gethostbyname(address);
	if(hostinfo == NULL)HERR("gethostbyname");
	addr.sin_addr = *(struct in_addr*) hostinfo->h_addr;
	return addr;
}

ssize_t bulk_read(int fd, char *buf, size_t count){
	int c;
	size_t len=0;
	do{
		//printf("Przed read\n");
		c=TEMP_FAILURE_RETRY(read(fd,buf,count));
		//printf("Po read, c: %d, buf: %s\n", c, buf);
		if(c<0) return c;
		if(0==c) return len;
		buf+=c;
		len+=c;
		count-=c;
	}while(count>0);
	return len ;
}

ssize_t bulk_write(int fd, char *buf, size_t count){
	int c;
	size_t len=0;
	do{
		c=TEMP_FAILURE_RETRY(write(fd,buf,count));
		if(c<0) return c;
		buf+=c;
		len+=c;
		count-=c;
	}while(count>0);
	return len ;
}

int connect_socket(char *name, uint16_t port){
	struct sockaddr_in addr;
	int socketfd;
	socketfd = make_socket();
	addr=make_address(name,port);
	if(connect(socketfd,(struct sockaddr*) &addr,sizeof(struct sockaddr_in)) < 0){
		if(errno!=EINTR) ERR("connect");
		else {
			fd_set wfds ;
			int status;
			socklen_t size = sizeof(int);
			FD_ZERO(&wfds);
			FD_SET(socketfd, &wfds);
			if(TEMP_FAILURE_RETRY(select(socketfd+1,NULL,&wfds,NULL,NULL))<0) ERR("select");
			if(getsockopt(socketfd,SOL_SOCKET,SO_ERROR,&status,&size)<0) ERR("getsockopt");
			if(0!=status) ERR("connect");
		}
	}
	printf("Połączono!\n");
	return socketfd;
}

int send_message(int fd, char* mess_type, char* mess) {
	int size;
	char* message;
	int c = 0;
	size = strlen(mess);

	message = malloc(strlen(mess_type) + 10 + size + 2);
	sprintf(message, "%s+%10d%s", mess_type, size, mess);

	printf("Przygotowana wiadomosc: %s\n", message);
	c = bulk_write(fd, message, strlen(message));
	free(message);
	return c;
}

void* read_message(int fd){
	char header_buff[HEADER_SIZE];
	char* message_buffer;
	char* message;
	char* mess_type;
	int *file_id, *part_size_int = 0;
	int mess_size;
    int c = 0;
	void* return_value;

	int i = 0;

    //header_buff = malloc(HEADER_SIZE);

    if((c = bulk_read(fd, header_buff, HEADER_SIZE)) != HEADER_SIZE) {
        if(c < 0)
        	ERR("read");
    }

	mess_type = strtok(header_buff, "+");
	mess_size = atoi(strtok(NULL, "+"));
	message_buffer = malloc(mess_size);
	message = malloc(mess_size);

	while((c = bulk_read(fd, message_buffer, mess_size)) != mess_size) {
        if(c < 0)
        	ERR("read");
    }

	//sprintf(message, "%s", message_buffer);
	printf("\n\n\n\nOTRZYMANA WIADOMOŚĆ: %s\n", message_buffer);

    switch(*mess_type) {
        case 82:
			part_size_int = malloc(1);
			*part_size_int = atoi(message_buffer);
			printf("Rozmiar wiadomości: %d\n", *part_size_int);
			return_value = (void*) part_size_int;
		break;
		case 70:
			file_id = malloc(1);
			*file_id = atoi(message_buffer);
			printf("Przysłany id pliku: %d", *file_id);
			return_value = (void*) file_id;
		break;
	}

	free(message);
	//free(header_buff);
	free(message_buffer);
    return return_value;
}

int wait_for_system_readiness(int fd) {
    int system_ready = 0;
    int* part_size;

    while(!system_ready && do_work) {
        part_size = (int*)read_message(fd);
        system_ready = *part_size;
    }

	free(part_size);
    return system_ready;
}

struct file_info* get_file_info(int fd, char* filename, int part_size) {
	struct file_info* file_info;
	int filesize, num_of_parts;
	FILE* fp;

	file_info = malloc(sizeof(struct file_info));

	fp = fopen(filename, "r");
	if (fp == NULL)
        ERR("fopen");

	fseek(fp, 0L, SEEK_END);
	filesize = ftell(fp);
	rewind(fp);

	num_of_parts = filesize/part_size;
	if(num_of_parts%part_size != 0) num_of_parts++;

	printf("Part size: %d, filesize: %d, num of parts: %d\n", part_size, filesize, num_of_parts);

	file_info -> file_size = filesize;
	file_info -> filename = filename;
	file_info -> num_of_parts = num_of_parts;

	fclose(fp);

	return file_info;
}

int send_file_info(int fd, char* filename, int part_size, struct file_info* file_info) {
	char mess_body[BUFF_SIZE];
	int c;
	int* file_id;

	sprintf(mess_body, "%s+%10d+%10d",file_info -> filename, file_info -> file_size, file_info -> num_of_parts);

	c = send_message(fd, "F", mess_body);

	sleep(1);

	file_id = (int*)read_message(fd);

	//free(file_info);
	return *file_id;
}

void send_part(int fd, char* part, int file_id, int part_id) {
	char* mess;
	mess= malloc(22+strlen(part));

	sprintf(mess, "%10d+%10d+%s", file_id, part_id, part);

	send_message(fd, "P", mess);
	free(mess);
}

void send_file(int fd, char* filename, struct file_info* file_info, int part_size) {
	FILE* fp;
	char part[BUFF_SIZE];
	char* message;
	int i = 0;

	fp = fopen(filename, "r");

	while(fgets(part, part_size+1, fp) != NULL)
   	{
		send_part(fd, part, file_info -> file_id, i++);
		/*
		printf("[CZĘŚĆ %d]: %s, długość: %d\n", i, part, strlen(part));
		if(bulk_write(fd, part, strlen(part)) < 0) ERR("bulk_write");
		i++;
		sleep(1);
		*/
	}

	fclose(fp);
}


int main(int argc, char** argv) {
    int fd, part_size, file_id;
	int k = 5;
	struct file_info* file_info;
	printf("%10d\n", k);


    sleep(2);

    if(argc!=4) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

    if(sethandler(SIG_IGN,SIGPIPE)) ERR("Seting SIGPIPE:");
    if(sethandler(sigint_handler,SIGINT)) ERR("Seting SIGINT:");

    fd = connect_socket(argv[1], atoi(argv[2]));

    part_size = wait_for_system_readiness(fd);

	file_info = get_file_info(fd, argv[3], part_size);

	if(( file_id = send_file_info(fd, argv[3], part_size, file_info)) < 0) ERR("write");

	file_info -> file_id = file_id;
	printf("File ID: %d\n\n\n\\n\n\n\n\n\n\n\\n\n\n\n\n\n\\n\n\n\n\n", file_info -> file_id);
	send_file(fd, argv[3], file_info, part_size);
	printf("File ID: %d\n\n\n\\n\n\n\n\n\n\n\\n\n\n\n\n\n\\n\n\n\n\n", file_info -> file_id);

	if(TEMP_FAILURE_RETRY(close(fd))<0) ERR("close");

	free(file_info);
    return 0;
}
