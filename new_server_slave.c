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

#include "structures.h"
#include "utils.h"
#include "read_message.h"

#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))
#define BUFF_SIZE 1024
#define HEADER_SIZE 12

volatile sig_atomic_t do_work=1;

struct uploader_info {
    int new_fd;
};

struct slave_info {
    int slave_id;
    int part_size;
};

void sigint_handler_ss(int sig);
int make_socket(int domain, int type);
int bind_inet_socket(uint16_t port,int type);
int add_new_client(int sfd);
ssize_t bulk_read(int fd, char *buf, size_t count);
ssize_t bulk_write(int fd, char *buf, size_t count);
void save_part_to_file(struct part_info* part_info, struct slave_info* slave_info);
char* read_part_from_file(int slave_id, int file_id, int part_id);
struct part_info* read_part_info_from_mess(char* buff);
struct slave_info* get_id(int fd);
void handle_messages(int fd, struct slave_info* slave_info);
int do_server(int fd);

void sigint_handler_ss(int sig) {
	do_work=0;
}

int make_socket(int domain, int type){
	int sock;
	sock = socket(domain,type,0);
	if(sock < 0) ERR("socket");
	return sock;
}

int bind_inet_socket(uint16_t port,int type){
	struct sockaddr_in addr;
	int socketfd,t=1;
	socketfd = make_socket(PF_INET,type);
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(&(addr.sin_zero), '\0', 8);



	//Allows to reuse port
	if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR,&t, sizeof(t)))
		ERR("setsockopt");

	//Binds port and all of the addresses
	if(bind(socketfd,(struct sockaddr*) &addr,sizeof(addr)) < 0)
		ERR("bind");

	if(SOCK_STREAM==type)
		if(listen(socketfd, 3) < 0) ERR("listen");
    //printf("server_slave: my data: %s\n", inet_ntoa(addr.sin_addr));

	return socketfd;
}

int add_new_client(int sfd){
	int nfd;
	if((nfd=TEMP_FAILURE_RETRY(accept(sfd,NULL,NULL)))<0) {
		if(EAGAIN==errno||EWOULDBLOCK==errno) return -1;
		ERR("accept");
	}
	return nfd;
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

int send_message(int fd, char* mess_type, char* mess) {
	int size;
	char* message;
	int c = 0;
	size = strlen(mess);

	message = malloc(strlen(mess_type) + 10 + size + 3);
	sprintf(message, "%s+%10d%s", mess_type, size, mess);

	c = bulk_write(fd, message, strlen(message));
	free(message);
	return c;
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

int send_part(char* buff, struct slave_info* slave, int fd) {
	char* file_id;
	char* part_id;
	char* part;
	char* mess;

	file_id = atoi(strtok(buff, "+"));
	part_id = atoi(strtok(NULL, "+"));

	printf("ODCZYTALEM FILE_ID: %d, PART_ID: %d\n\n\n\n", file_id, part_id);

	part = read_part_from_file(slave -> slave_id, file_id, part_id);

	//mess = malloc(23 + strlen(part));
	//sprintf(mess, "%10d+%10d+%s", file_id, part_id, part);
	mess = malloc(strlen(part));
	sprintf(mess, "%s", part);

	send_message(fd, "D", mess);

	printf("Wysłana wiadomość: %s\n\n\n", mess);

	//printf("A OTOTOTOOT ODCZYTANY PART: %s", part);

}

struct slave_info* read_slave_info_from_mess(struct message* mess) {
	char* id, part_size, buff_copy;
	struct slave_info* slave_info;

	printf("[READ SLAVE] buff: %s, size: %d\n", mess -> message, strlen(mess -> message));

	//buff_copy = malloc(strlen(mess -> message) + 1);
	//strcpy(buff_copy, mess -> message);

	id = strtok(mess -> message, "+");
	part_size = strtok(NULL, "+");
	slave_info = malloc(sizeof(struct slave_info));

	//printf("[READ SLAVE] id: %s\n", id);
	//printf("[READ SLAVE] id: %s\n", part_size);
	slave_info -> slave_id = atoi(strtok(mess -> message, "+"));
	slave_info -> part_size = atoi(strtok(mess -> message, "+"));

	return slave_info;
}

struct slave_info* get_id(int fd) {
    struct slave_info* slave_info;
	struct message* message;

	message = malloc(sizeof(struct message));
	read_mess(fd, message);

	slave_info = read_slave_info_from_mess(message);

	printf("[ID: %d] Otrzymałem part size: %d\n", slave_info -> slave_id, slave_info -> part_size);

    return slave_info;
}

void save_info_about_part_slave(struct part_info* part_info, struct slave_info* slave_info) {
	char slave_part_info_path[BUFF_SIZE];
	char slave_part_info_file[BUFF_SIZE];
	char line[BUFF_SIZE];
	FILE* fp;

	sprintf(slave_part_info_path, "tmp/file_info/%d/%d/slaves", part_info -> file_id, part_info -> part_id);

	if(mkpath(slave_part_info_path, 0755) < 0)
		ERR("mkpath");

	fp = fopen(slave_part_info_path, "ab+");

	sprintf(line, "%d\n", slave_info -> slave_id);

	fputs(line, fp);

	fclose(fp);
}

void save_part_to_file(struct part_info* part_info, struct slave_info* slave_info){
	char part_path[BUFF_SIZE];
	FILE* fp;

	sprintf(part_path, "tmp/parts/%d/%d/%d", slave_info -> slave_id, part_info -> file_id, part_info -> part_id);

	if(mkpath(part_path, 0755) < 0)
		ERR("mkpath");

	fp = fopen(part_path, "ab+");

	fputs(part_info -> data, fp);

	fclose(fp);

	save_info_about_part_slave(part_info, slave_info);
}

char* read_part_from_file(int slave_id, int file_id, int part_id) {
	char part_path[BUFF_SIZE];
	FILE* fp;
	char* part;
	long fsize;

	sprintf(part_path, "tmp/parts/%d/%d/%d", slave_id, file_id, part_id);

	fp = fopen(part_path, "rb");

	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);  //same as rewind(f);

	part = malloc(fsize + 1);
	fread(part, fsize, 1, fp);
	fclose(fp);

	part[fsize] = 0;

	return part;
}

void handle_messages(int fd, struct slave_info* slave_info) {
	struct part_info* part_info;
	struct message* message;
	message = malloc(sizeof(struct message));

	while(do_work) {
		read_mess(fd, message);
		switch(*(message -> type)){
			case 80:
				part_info = read_part_info_from_mess(message -> message);
				save_part_to_file(part_info, slave_info);
				break;
			case 68:
				send_part(message -> message, slave_info, fd);
				break;
		}
        printf("Czekam na wiadomość\n");
        sleep(0.1);
    }
}

int do_server(int fd) {
    int new_fd = 0;
    struct slave_info* slave_info;

    printf("OCZEKUJĘ NA POŁĄCZENIE\n");
    while((new_fd = add_new_client(fd)) < 0) {
        sleep(1);
    }

    printf("POŁĄCZONO!\n");

    slave_info = get_id(new_fd);

    handle_messages(new_fd, slave_info);

    close(new_fd);

    return 0;
}

int main(int argc, char** argv) {
    int fd;
    int new_flags;

    if(sethandler(SIG_IGN,SIGPIPE)) ERR("Seting SIGPIPE:");
	if(sethandler(sigint_handler_ss,SIGINT)) ERR("Seting SIGINT:");

    fd=bind_inet_socket(atoi(argv[1]),SOCK_STREAM);
    new_flags = fcntl(fd, F_GETFL) | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_flags);

    if(do_server(fd) < 0) {
        ERR("do_server:");
    }

    close(fd);

    return EXIT_SUCCESS;
}
