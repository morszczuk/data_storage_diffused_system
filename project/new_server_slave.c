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

#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))
#define BUFF_SIZE 1024

volatile sig_atomic_t do_work=1;

struct uploader_info {
    int new_fd;
};

struct slave_info {
    int slave_id;
    int part_size;
};

void usage(char * name);
int sethandler(void (*f)(int), int sigNo);
void sigint_handler(int sig);
int make_socket(int domain, int type);
int bind_inet_socket(uint16_t port,int type);
int add_new_client(int sfd);
struct slave_info* get_id(int fd);
void handle_messages(int fd);
int do_server(int fd);

void usage(char * name){
	fprintf(stderr,"USAGE: %s port\n",name);
}

int sethandler( void (*f)(int), int sigNo) {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if (-1==sigaction(sigNo, &act, NULL))
		return -1;
	return 0;
}

void sigint_handler(int sig) {
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

void* read_message(char* buff) {
    char* mess;
    char* mess_type;
    char* id;
    char* part_size;
    mess_type = malloc(BUFF_SIZE);
    struct slave_info* slave_info;


    mess = malloc(strlen(buff));
    sprintf(mess, "%s", buff);

    mess_type = strtok(mess, "+");
    switch(*mess_type) {
        case 82:
            id = strtok(NULL, "+");
            part_size = strtok(NULL, "+");
            slave_info = malloc(sizeof(struct slave_info));
            slave_info -> slave_id = atoi(id);
            slave_info -> part_size = atoi(part_size);
            free(mess);
            free(id);
            free(slave_info);
            return (void*)slave_info;
        break;
    }

    printf("CHECK 3 \n");


    free(mess);
    return (void*)NULL;
}

struct slave_info* get_id(int fd) {
    int c;
    char buff[BUFF_SIZE];
    int id;
    struct slave_info* slave_info;

    c=TEMP_FAILURE_RETRY(read(fd,&buff,BUFF_SIZE));
    if(c < 0)
        ERR("read");
    if(c >= 0) {
        slave_info = (struct slave_info*)read_message(buff);
        printf("[ID: %d] Otrzymałem part size: %d\n", slave_info -> slave_id, slave_info -> part_size);
    }

    return slave_info;
}

void handle_messages(int fd) {
    while(do_work) {
        printf("Czekam na wiadomość\n");
        sleep(1);
    }
}

int do_server(int fd) {
    int new_fd = 0;
    struct sockaddr_in* addr;
    socklen_t size;
    struct slave_info* slave_info;

    printf("OCZEKUJĘ NA POŁĄCZENIE\n");
    while((new_fd = add_new_client(fd)) < 0) {
        sleep(1);
    }

    printf("POŁĄCZONO!\n");

    slave_info = get_id(new_fd);

    handle_messages(new_fd);

    close(new_fd);

    return 0;
}

int main(int argc, char** argv) {
    int fd;
    int new_flags;

    if(sethandler(SIG_IGN,SIGPIPE)) ERR("Seting SIGPIPE:");
	if(sethandler(sigint_handler,SIGINT)) ERR("Seting SIGINT:");

    fd=bind_inet_socket(atoi(argv[1]),SOCK_STREAM);
    new_flags = fcntl(fd, F_GETFL) | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_flags);

    if(do_server(fd) < 0) {
        ERR("do_server:");
    }

    close(fd);

    return EXIT_SUCCESS;
}
