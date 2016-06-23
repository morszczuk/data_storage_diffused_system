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

#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))
#define BUFF_SIZE 1024

volatile sig_atomic_t do_work=1;
pthread_mutex_t lock;
int system_ready = 0;

//structures
struct connection_arg {
    char* file;
    int sock;
	struct slave_node* slaves_list;
	int part_size;
};

struct slave_node {
	int slave_id;
	int port;
	char* addr;
	int sock;
	pthread_t connection_tid;
	struct slave_node* next;
	int part_size;
};

struct uploader_thread {
	struct uploader_thread* next;
	pthread_t tid;
	int new_fd;
	int fd;
	int part_size;
};

//functions headers
void usage(char* name);
void sigint_handler(int sig);
int sethandler( void (*f)(int), int sigNo);
struct sockaddr_in make_address(char *address, uint16_t port);
int make_socket(int domain, int type);
int bind_inet_socket(uint16_t port, int type);
int add_new_client(int sfd);
ssize_t bulk_write(int fd, char *buf, size_t count);
char* make_number_string(int k);
int get_number_of_slaves(char* filename);
void* slaves_establish_connection_job(void* args);
void* slave_connect(void* args);
int slaves_establish_connection(int fd, char* filename, struct slave_node* slaves_list,
	struct connection_arg* arg, int part_size);
void read_slaves_from_file(char* file, struct slave_node* slaves, int part_size);
int handle_upload_clients(int fd, int part_size);
void send_size_mess(int fd, int part_size);
void wait_for_system_readiness(int fd);
void* manage_upload(void* args);

//fucntions definitions
void usage(char* name){
    fprintf(stderr,"USAGE: %s port slaves_list_file system_reliability size_part\n",name);
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

struct sockaddr_in make_address(char *address, uint16_t port){
	struct sockaddr_in addr;
	struct hostent *hostinfo;
	addr.sin_family = AF_INET;
	addr.sin_port = htons (port);
	hostinfo = gethostbyname(address);
	if(hostinfo == NULL) ERR("gethostbyname");
	addr.sin_addr = *(struct in_addr*) hostinfo->h_addr;
	return addr;
}

int make_socket(int domain, int type){
	int sock;
	sock = socket(domain,type,0);
	if(sock < 0) ERR("socket");
	return sock;
}

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

char* make_number_string(int k) {
	int i = 0;
	int j = 0;
	char *number = (char *) malloc(sizeof(char) * 10);
	char *result = (char *) malloc(sizeof(char) * 10);

	for(i =0; i < 10; i++)
		result[i] = '0';

	sprintf(number, "%d", k);

	for(i = strlen(number)-1; i >=0; i--) {
		result[9-j] = number[i];
		j++;
	}

	free(number);
	return result;
}

int get_number_of_slaves(char* filename) {
	int lines = 0;
	FILE* fp;
	int ch;

	fp = fopen(filename, "r");
	if (fp == NULL)
		ERR("fopen");
	while(!feof(fp))
	{
	  ch = fgetc(fp);
	  if(ch == '\n')
	  {
	    lines++;
	  }
	}
	fclose(fp);
	return lines;
}

void read_slaves_from_file(char* file, struct slave_node* slaves, int part_size) {
	FILE* fp;
	char* line;
	size_t len = 0;
	struct slave_node* slave_node = 0;
	int read = 0;

	//printf("malloc++\n");
	line = malloc(BUFF_SIZE);

	fp = fopen(file, "r");
	if (fp == NULL)
        ERR("fopen");

	printf("CHECK 3\n");
	while ((read = getline(&line, &len, fp)) != -1) {
		printf("CHECK 4\n");
		if(slaves -> slave_id == -1) {
			slave_node = slaves;
			//printf("malloc++\n");
			slave_node -> next = malloc(sizeof(struct slave_node));
			slave_node -> next -> slave_id = -1;
		} else {
			slave_node = slave_node -> next;
			//printf("malloc++\n");
			slave_node -> next = malloc(sizeof(struct slave_node));
			slave_node -> next -> slave_id = -1;
		}

		printf("CHECK 5\n");
		slave_node -> slave_id = atoi(strtok(line," "));
		slave_node -> addr = strtok(NULL, " ");
		slave_node -> port = atoi(strtok (NULL, " "));
		slave_node -> part_size = part_size;
    }

	fclose(fp);
	free(line);
}

void* slaves_establish_connection_job(void* args) {
	struct connection_arg* arg;
	arg = (struct connection_arg*)args;
	struct slave_node* node;
	printf("CHECK 2\n");
	read_slaves_from_file(arg -> file, arg -> slaves_list, arg -> part_size);
	printf("CHECK 6\n");
	node = arg -> slaves_list;
	printf("CHECK 7\n");

	while(node -> slave_id != -1) {
		printf("CHECK 8\n");
		printf("[SLAVE %d] PORT: %d\n", node -> slave_id, node -> port);
		if((pthread_create(&(node -> connection_tid), NULL, slave_connect, (void*)node)) < 0)
			ERR("pthread create:");
		node = node -> next;
		printf("CHECK 9\n");
	}

	node = arg -> slaves_list;
	while(node -> slave_id != -1) {
		pthread_join(node -> connection_tid, NULL);
		node = node -> next;
	}
	printf("CHECK 10\n");
	printf("System gotowy do działania!\n");
	pthread_mutex_lock(&lock);
	system_ready = 1;
	pthread_mutex_unlock(&lock);


	return NULL;
}

void* slave_connect(void* args) {
	struct slave_node* slave;
	struct sockaddr_in addr;
	int sock;
	char mess[23];
	char* slave_id, *part_size;

	slave = (struct slave_node*)args;

	addr = make_address(slave -> addr, slave -> port);
	sock = make_socket(PF_INET, SOCK_STREAM);
	printf("[ID: %d][%s %d] Czekam na połączenie.\n", slave-> slave_id, slave -> addr, slave -> port);
	while( TEMP_FAILURE_RETRY(connect(sock,(struct sockaddr*) &addr,sizeof(struct sockaddr_in))) != 0) {
		sleep(1);
		ERR("connect");
	}

	slave -> sock = sock;

	slave_id = make_number_string(slave -> slave_id);
	part_size = make_number_string(slave -> part_size);
	sprintf(mess, "%s+%s+%s", "R", slave_id, part_size);

	if(bulk_write(sock, mess, strlen(mess))< 0) ERR("write:");

	printf("[ID: %d][%s %d] Połączono.\n", slave -> slave_id, slave -> addr, slave -> port);

	free(slave_id);
	free(part_size);

	return NULL;
}

int slaves_establish_connection(int fd, char* filename, struct slave_node* slaves_list, struct connection_arg* arg, int part_size) {
	pthread_t tid = 0;

	arg -> file = filename;
	arg -> sock = fd;
	arg -> slaves_list = slaves_list;
	arg -> part_size = part_size;

	printf("CHECK 1\n");
	if((pthread_create(&(tid), NULL, slaves_establish_connection_job, (void*)arg)) < 0)
		ERR("pthread create:");

	return tid;
}

int handle_upload_clients(int fd, int part_size) {
	int new_fd;
	struct uploader_thread* upload_threads, *up_thread;

	upload_threads = malloc(sizeof(struct uploader_thread));
	upload_threads -> fd = -1;
	up_thread = upload_threads;

	while(do_work) {
		if((new_fd = add_new_client(fd)) > 0) {
			if(up_thread -> fd != -1 )
				up_thread = up_thread -> next;

			up_thread -> next = malloc(sizeof(struct uploader_thread));
			up_thread -> next -> fd = -1;

			up_thread -> fd = fd;
			up_thread -> new_fd = new_fd;
			up_thread -> part_size = part_size;

			if((pthread_create(&(up_thread -> tid), NULL, manage_upload, (void*)up_thread)) < 0)
				ERR("pthread_create:");
		}
		sleep(1);
	}

	up_thread = upload_threads;
	while(up_thread -> fd != -1) {
		pthread_join(up_thread -> tid, NULL);
	}

	while(upload_threads -> fd != -1) {
		up_thread = upload_threads;
		upload_threads = upload_threads -> next;
		free(up_thread);
	}

	free(upload_threads);
	return 0;
}

void send_size_mess(int fd, int part_size) {
	char* mess;
	mess = malloc(BUFF_SIZE);

	sprintf(mess, "%s+%d", "R", part_size);
	if(bulk_write(fd, mess, strlen(mess)) < 0)
		ERR("bulk_write");
}

void wait_for_system_readiness(int fd) {
	int is_system_ready = 0;

	pthread_mutex_lock(&lock);
	is_system_ready = system_ready;
	pthread_mutex_unlock(&lock);

	while(!is_system_ready && do_work) {
		send_size_mess(fd, 0);
		sleep(1);
		pthread_mutex_lock(&lock);
		is_system_ready = system_ready;
		pthread_mutex_unlock(&lock);
	}

}

void* manage_upload(void* args) {
	struct uploader_thread* uploader;

	uploader = (struct uploader_thread*) args;

	close(uploader -> fd);
	wait_for_system_readiness(uploader -> fd);

	if(do_work)
		send_size_mess(uploader -> new_fd, uploader -> part_size);

	return NULL;
}

int main(int argc, char** argv) {
	pthread_t establish_connection_tid, uploader_tid;
	struct connection_arg* arg = 0;
	struct slave_node* slaves_list = 0;
	struct slave_node* node = 0;
    int fd, new_flags;
    int system_reliability = 0;
    int slaves_count = 0;
	int part_size = 10;

    srand(time(NULL));

    if(argc<3) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

    if(argc == 4) system_reliability = atoi(argv[3]);

	if(argc== 5) part_size = atoi(argv[4]);

    if(sethandler(SIG_IGN,SIGPIPE)) ERR("Seting SIGPIPE:");
	if(sethandler(sigint_handler,SIGINT)) ERR("Seting SIGINT:");
    if (pthread_mutex_init(&lock, NULL) != 0) ERR("Mutex init:");

    if((slaves_count = get_number_of_slaves(argv[2])) <= system_reliability) {
		printf("Zbyt mało slave'ów aby system działał!\n");
		exit(EXIT_FAILURE);
	}

    fd = bind_inet_socket(atoi(argv[1]),SOCK_STREAM);

    new_flags = fcntl(fd, F_GETFL) | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_flags);

	slaves_list = malloc(sizeof(struct slave_node));
	printf("malloc++\n");
	slaves_list -> slave_id = -1;
	arg = malloc(sizeof(struct connection_arg));
	printf("malloc++\n");

	establish_connection_tid = slaves_establish_connection(fd, argv[2], slaves_list, arg, part_size);
	uploader_tid = handle_upload_clients(fd, part_size);

	pthread_join(establish_connection_tid, NULL);
	pthread_join(uploader_tid, NULL);

	while(slaves_list -> slave_id != -1 ) {
		node = slaves_list;
		slaves_list = slaves_list -> next;
		close(node -> sock);
		free(node);
	}

	if(TEMP_FAILURE_RETRY(close(fd))<0) ERR("close");

	free(slaves_list);
	free(arg);

	exit(EXIT_SUCCESS);
}
