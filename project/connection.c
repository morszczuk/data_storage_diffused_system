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

#include "connection.h"

struct sockaddr_in make_address(char *address, uint16_t port){
	struct sockaddr_in addr;
	struct hostent *hostinfo;
	addr.sin_family = AF_INET;
	addr.sin_port = htons (port);
	hostinfo = gethostbyname(address);
	printf("PRZED HOST\n");
	if(hostinfo == NULL) ERR("gethostbyname");
	printf("PO HOST\n");
	addr.sin_addr = *(struct in_addr*) hostinfo->h_addr;
	return addr;
}

int make_socket(int domain, int type){
	int sock;
	sock = socket(domain,type,0);
	if(sock < 0) ERR("socket");
	return sock;
}

/*//////////////////////////////////////////////
Creates socket and binds it to the specific port
//////////////////////////////////////////////*/
int bind_inet_socket(uint16_t port,int type) {
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

ssize_t bulk_read(int fd, char *buf, size_t count){
	int c;
	size_t len=0;
	do{
		c=TEMP_FAILURE_RETRY(read(fd,buf,count));
		if(c<0) return c;
		if(0==c) return len;
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

void* read_message_sm(int fd){
	char *header_buff, *message_buffer, *mess_type, *mess_size_s;
	int mess_size, c = 0;
	void* return_value;
	struct file_info *file_info;
	struct part_info *part_info;

    header_buff = malloc(HEADER_SIZE+1);
	//mess_size_s = malloc(11);

    while((c = read(fd, header_buff, HEADER_SIZE)) < 0) {
        if(c < 0)
        	ERR("read");
    }

	header_buff[HEADER_SIZE] ='\0';
	mess_type = strtok(header_buff, "+");
	mess_size = atoi(strtok(NULL, "+"));
	message_buffer = malloc(mess_size+1);

	if((c = bulk_read(fd, message_buffer, mess_size)) != mess_size) {
        if(c < 0)
        	ERR("read");
    }

	message_buffer[mess_size] = '\0';

	printf("OTRZYMANA WIADOMOŚĆ: %s\n", message_buffer);

    switch(*mess_type) {
		case 70:
			file_info = malloc(sizeof(struct file_info));
			read_file_info_from_mess(file_info, message_buffer);
			return_value = (void*)file_info;
		break;
		case 80:
			part_info = read_part_info_from_mess(message_buffer);
			return_value = (void*)part_info;
		break;
	}
	printf("W poszukiwaniu błędu 1\n");
	free(header_buff);
	printf("W poszukiwaniu błędu 2\n");
	free(message_buffer);
	printf("W poszukiwaniu błędu 3\n");
	//free(mess_type);
	printf("W poszukiwaniu błędu 4\n");
	//free(mess_size_s);
	printf("W poszukiwaniu błędu 5\n");

	printf("Wszystko zwolnione\n");
    return return_value;
}
