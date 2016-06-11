#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <pthread.h>
#include <regex.h>
#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))
#define HERR(source) (fprintf(stderr,"%s(%d) at %s:%d\n",source,h_errno,__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))
#define BUFF_SIZE 3000
#define PART_SIZE 5

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
	return socketfd;
}

ssize_t bulk_read(int fd, char *buf, size_t count){
	int c;
	size_t len=0;
	printf("BULK READ, count: %d\n", count);
	do{
		c=TEMP_FAILURE_RETRY(read(fd,buf,count));
		printf("Tyle przeczytalo: %d\n", c);
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

void prepare_request(char **argv,int32_t data[5]){
	data[0]=htonl(atoi(argv[3]));
	data[1]=htonl(atoi(argv[4]));
	data[2]=htonl(0);
	data[3]=htonl((int32_t)(argv[5][0]));
	data[4]=htonl(1);
}

void print_answer(int32_t data[5]){
	if(ntohl(data[4]))
		printf("%d %c %d = %d\n", ntohl(data[0]),(char)ntohl(data[3]), ntohl(data[1]), ntohl(data[2]));
	else printf("Operation impossible\n");
}

void usage(char * name){
	fprintf(stderr,"USAGE: %s domain port \n",name);
}

void send_file(int fd, char* filename) {
	printf("Przechodzę do wysyłki pliku\n");
	int fp, sz, parts_count;
	char part[PART_SIZE];
	char parts_c[50];
	pthread_t tid;
	int i = 1;


	fp = fopen(filename, "r");
	if (fp == NULL)
        ERR("fopen");

	fseek(fp, 0L, SEEK_END);
	sz = ftell(fp);
	rewind(fp);

	parts_count = sz/PART_SIZE;

	if(parts_count%PART_SIZE != 0)
		parts_count++;

	printf("Liczba części wykryta: %d\n", parts_count);
	sprintf(parts_c, "%d", parts_count);

	if(bulk_write(fd, parts_c, strlen(parts_c)) < 0) ERR("bulk_write");

	sleep(3);

	while(fgets(part, PART_SIZE+1, fp) != NULL)
   	{
		printf("[CZĘŚĆ %d]: %s\n", i, part);
		if(bulk_write(fd, part, strlen(part)) < 0) ERR("bulk_write");
		i++;
	}
}

int main(int argc, char** argv) {
	int fd, c;
	char data[BUFF_SIZE];
	int server_ready = 0;

	if(argc!=4) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	if(sethandler(SIG_IGN,SIGPIPE)) ERR("Seting SIGPIPE:");
	fd=connect_socket(argv[1],atoi(argv[2]));

	//prepare_request(argv,data);
	/*
	 * Broken PIPE is treated as critical error here
	 */
     //if(bulk_read(fd,(char *)data,100)<(100)) ERR("read:");
	//if(bulk_write(fd,(char *)data,strlen(data))<0) ERR("write:");
	//printf("Zmieniłem coś\n");
	printf("Sprawdzam czy serwer jest gotowy\n");
	while(!server_ready){
		memset(data,0,sizeof(data));
		printf("Jeszcze nie\n");
		c=TEMP_FAILURE_RETRY(read(fd,&data,BUFF_SIZE));
		if(c < 0)
			ERR("read");
		else {
			server_ready = atoi(&data);
		}
		sleep(1);
	}
	printf("System gotowy!\n");

	server_ready = 0;

	send_file(fd, argv[3]);

	while(!server_ready) {
		memset(data,0,sizeof(data));
		printf("Czekam na przeczytanie\n");
		c=TEMP_FAILURE_RETRY(read(fd,&data,BUFF_SIZE));
		if(c < 0)
			ERR("read");
		else {
			server_ready = atoi(&data);
		}
		sleep(1);
	}

	printf("Jestem szczęśliwy, kończę działanie\n" );
	//printf("%s\n", data);
    //print_answer(data);
	if(TEMP_FAILURE_RETRY(close(fd))<0)ERR("close");
	return EXIT_SUCCESS;
}
