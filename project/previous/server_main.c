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

#define BACKLOG 3
#define BUFF_SIZE 3000
#define PART_SIZE 10

volatile sig_atomic_t do_work=1 ;
void* connect_to_slave(void* args);
void* uploader_listener(void* args);
void* send_file_part_to_slaves(void* args);
void* manage_upload(void* args);
int get_number_of_slaves(char* filename);
char*  make_number_string(int k);
pthread_t upload_listener_tid;
int slaves_connected = 0;
struct slave* slave_addresses;
int slaves_count;
int system_lock = 0;
int file_count = 0;
void wait_for_slaves(int new_fd);

pthread_mutex_t lock;

void sigint_handler(int sig) {
	do_work=0;
}

struct server_slave_info {
	int port;
	char* address;
	int socket;
};

struct slave {
	char* port;
	char* addr;
	int sock;
	char* slave_id;
};

struct slave_arg {
	char* part;
	int i;
	int file_id;
	int part_id;
};

struct listener_thread {
	struct listener_thread* next;
	pthread_t tid;
	struct manage_upload_args* args;
};

struct manage_upload_args {
	int new_fd;
};

int sethandler( void (*f)(int), int sigNo) {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if (-1==sigaction(sigNo, &act, NULL))
		return -1;
	return 0;
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
		if(listen(socketfd, BACKLOG) < 0) ERR("listen");

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

void usage(char * name){
	fprintf(stderr,"USAGE: %s port slaves_list_file system_const\n",name);
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

void establish_connection_with_slaves(char* file, int sock) {
	FILE* fp;
	char* line;
	char* address;
	char* port;
	char* id;
	size_t len = 0;
	ssize_t read;
	int k, i = 0;
	pthread_t* tids;

	slave_addresses = (struct slave*)calloc(slaves_count, sizeof(struct slave));
	tids = (pthread_t*)calloc(slaves_count, sizeof(pthread_t));

	fp = fopen(file, "r");
	if (fp == NULL)
        ERR("fopen");

	while ((read = getline(&line, &len, fp)) != -1) {
	  	id = strtok(line," ");
		address = strtok(NULL, " ");
		port = strtok (NULL, " ");
		printf("Connecting to: %s %s", address, port);
		slave_addresses[i].port = port;
		slave_addresses[i].addr = address;
		slave_addresses[i].slave_id = id;
		if((pthread_create(&(tids[i]), NULL, connect_to_slave, (void*)&slave_addresses[i])) < 0)
			ERR("pthread_create:");
		pthread_join(tids[i], (void**)&((slave_addresses[i]).sock));
		i++;
    }

	pthread_mutex_lock(&lock);
	slaves_connected = 1;
	pthread_mutex_unlock(&lock);
	for(k = 0; k < i; k++) {
		printf("Zwrócono socket: [%d]\n", (slave_addresses[k]).sock);
	}


	/*
	for(k = 0; k < i; k++) {
		printf("CHEKPOINT 4/%d\n", k);
		pthread_join(tids[k], NULL);
	}	*/

	//free(tids);
	fclose(fp);
}

void* connect_to_slave(void* arg) {
	int sock;
	struct slave* slave_addr;
	struct sockaddr_in addr;

	slave_addr = (struct slave*) arg;
	addr = make_address((*slave_addr).addr, atoi((*slave_addr).port));
	sock = make_socket(PF_INET, SOCK_STREAM);
	while(TEMP_FAILURE_RETRY(connect(sock,(struct sockaddr*) &addr,sizeof(struct sockaddr_in))) < 0) {
		sleep(1);
		//printf("Połączenie nie zostało nawiązane, ponawiam\n");
	}
		//if(errno!=EINTR) ERR("connect");

	printf("CONNECTED PORT: %s. TRYING TO SEND: %S\n", (*slave_addr).port, slave_addr -> slave_id);

	bulk_write(sock, slave_addr -> slave_id, strlen(slave_addr -> slave_id));

	return (void*)sock;
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

void* uploader_listener(void* args) {


	/*ARGUMENTY POTRZEBNE W TEJ FUNKCJI */
	int fd, new_fd;
	struct listener_thread* head;
	struct listener_thread* lt;
	int new_flags;

	fd = (int)args;
	//new_flags = fcntl(fd, F_GETFL) | O_NONBLOCK;
	fcntl(fd, F_SETFL, O_NONBLOCK);

	while(do_work) {

		if( (new_fd = add_new_client(fd)) > 0 ) {
			if( head == NULL) {
				head = malloc(sizeof(struct listener_thread));
				head -> tid = NULL;
				head -> next = NULL;
				head -> args = malloc(sizeof(struct manage_upload_args));
				lt = head;
			}
			else {
				lt -> next = malloc(sizeof(struct listener_thread));
				lt = lt-> next;
				lt -> tid = NULL;
				lt -> next = NULL;
				lt -> args = malloc(sizeof(struct manage_upload_args));
			}

			lt -> args -> new_fd = new_fd;

			if((pthread_create(&(lt -> tid), NULL, manage_upload, (void*)lt -> args)) < 0)
				ERR("pthread_create:");
		}
		sleep(1);
	}

	lt = head;

	while(lt != NULL) {
		pthread_join(lt-> tid, NULL);
		lt = lt->next;
	}

	//printf("KOŃCZĘ PROGRAM\n");
	/*

	mess_waiting = "0";
	mess_ready = "1";
	printf("Mam połączenie z uploaderem\n");



	while(!are_slaves_connected && do_work) {

		pthread_mutex_lock(&lock);
		are_slaves_connected = slaves_connected;
		pthread_mutex_unlock(&lock);
		//printf("System czeka na nawiązanie połączenia ze wszystkimi serwerami slave. Proszę czekać\n");
		//if ((recv(new_fd, buff, 100, 0)) == -1) ERR("recv:");
		printf("strlen(mess): %d\n", strlen(mess_waiting));
		//if((c=TEMP_FAILURE_RETRY(write(new_fd,&mess,strlen(mess))))< 0) ERR("write:");
		if(bulk_write(new_fd, mess_waiting, strlen(mess_waiting)) < 0) ERR("bulk_write");
		//if(bulk_read(new_fd, buff, 7) > 0)
			//printf("%s\n", buff);
			//close(new_fd);


		//buff = 0;
		//bulk_write(new_fd, mess, sizeof(mess));
		//printf("CHECK 1\n");
		sleep(2);
		//printf("CHECK 2\n");
	}

	//if(bulk_read(new_fd, buff, 7) > 0)
		//printf("%s\n", buff);
	if(bulk_write(new_fd, mess_ready, strlen(mess_ready))< 0) ERR("write:");

	sleep(3);
	memset(buff,0,sizeof(buff));

	c=TEMP_FAILURE_RETRY(read(new_fd,&buff,BUFF_SIZE));
	if(c < 0)
		ERR("read");
	else {
		printf("Liczba części: %s\n", &buff);
		number_of_parts = atoi(&buff);
	}

	sleep(2);
	tids = (pthread_t*)calloc(number_of_parts, sizeof(pthread_t));
	slave_args = (struct slave_arg*)calloc(number_of_parts, sizeof(struct slave_arg));

	while(number_of_parts > 0) {

		memset(buff,0,sizeof(buff));
		//printf("Waiting for contact from uploader\n");
		//bulk_write(new_fd, mess, strlen(mess));
		c=TEMP_FAILURE_RETRY(read(new_fd,&buff,PART_SIZE));
		if(c < 0)
			ERR("read");
		else {

			num = rand()%(slaves_count - system_lock - 1);

			slave_args[i].part = malloc(strlen(&buff));
			strcpy(slave_args[i].part, &buff);
			slave_args[i].i = num;
			slave_args[i].file_id = 0;
			slave_args[i].part_id = i;

			if((pthread_create(&(tids[i]), NULL, send_file_part_to_slaves, (void*)&slave_args[i])) < 0)
				ERR("pthread_create:");
		}

		number_of_parts--;
		i++;
	}

	for(i = 0; i<number_of_parts; i++)
	{
		pthread_join(tids[i], NULL);
	}

	if(bulk_write(new_fd, mess_ready, strlen(mess_ready))< 0) ERR("write:");
	*/
	return 0;

}

void* manage_upload(void* args) {

	char buff[BUFF_SIZE];
	int new_fd;
	int c;
	int number_of_parts;
	pthread_t* tids;
	int num;
	struct slave_arg* slave_args;
	int i = 0;
	char mess_ready[10];
	struct manage_upload_args* data;
	data = (struct manage_upload_args*)args;
	int saved_flags;

	number_of_parts = 18;

	new_fd = data -> new_fd;

	sprintf(mess_ready, "%d", PART_SIZE);

	//saved_flags = fcntl(data-> new_fd, F_GETFL);
	//fcntl(data -> new_fd, saved_flags & ~ O_NONBLOCK);

	wait_for_slaves(data -> new_fd);

	printf("Serwer gotowy! Rozmiar części: %s\n", mess_ready);
	if(bulk_write(new_fd, mess_ready, strlen(mess_ready))< 0) ERR("write:");
	printf("Piszę teraz, deskryptor: %d\n", new_fd);
	//sleep(3);
	memset(buff,0,sizeof(buff));
	printf("Czekam teraz, deskryptor: %d, bufor: [%s]\n", new_fd, buff);
	/*if((c=TEMP_FAILURE_RETRY(recv(data -> new_fd,&buff,BUFF_SIZE, 0))) < 0)
		ERR("read");
	printf("c: %d\n", c);*/
	c = TEMP_FAILURE_RETRY(read(new_fd, &buff, BUFF_SIZE));
	if(c < 0)
		ERR("read");
	else {
		printf("Liczba części: %s\n", &buff);
		number_of_parts = atoi(&buff);
	}
	printf("Dostalem informacje o czesciach\n" );

	tids = (pthread_t*)calloc(number_of_parts, sizeof(pthread_t));
	slave_args = (struct slave_arg*)calloc(number_of_parts, sizeof(struct slave_arg));

	printf("Dojdzie tutaj? 2\n");
	printf("number_of_parts: %d", number_of_parts);

	while(number_of_parts > 0) {

		memset(buff,0,sizeof(buff));
		//printf("Waiting for contact from uploader\n");
		//bulk_write(new_fd, mess, strlen(mess));
		c=TEMP_FAILURE_RETRY(read(data -> new_fd,&buff,PART_SIZE));
		if(c < 0)
			ERR("read");
		else {

			num = rand()%(slaves_count - system_lock - 1);

			slave_args[i].part = malloc(strlen(&buff));
			strcpy(slave_args[i].part, &buff);
			slave_args[i].i = num;
			slave_args[i].file_id = file_count;
			slave_args[i].part_id = i;

			if((pthread_create(&(tids[i]), NULL, send_file_part_to_slaves, (void*)&slave_args[i])) < 0)
				ERR("pthread_create:");
		}

		number_of_parts--;
		i++;
	}

	printf("Dojdzie tutaj? 3\n");

	for(i = 0; i<number_of_parts; i++)
	{
		pthread_join(tids[i], NULL);
	}

	file_count++;

	printf("Dojdzie tutaj? 1\n");

	if(bulk_write(new_fd, mess_ready, strlen(mess_ready))< 0) ERR("write:");


	return NULL;
}

void wait_for_slaves(int new_fd) {
	int are_slaves_connected;
	char* mess_waiting = "0";

	pthread_mutex_lock(&lock);
	are_slaves_connected = slaves_connected;
	pthread_mutex_unlock(&lock);

	while(!are_slaves_connected && do_work) {

		pthread_mutex_lock(&lock);
		are_slaves_connected = slaves_connected;
		pthread_mutex_unlock(&lock);

		if(bulk_write(new_fd, mess_waiting, strlen(mess_waiting)) < 0) ERR("bulk_write");
		sleep(1);
	}
}

void* send_file_part_to_slaves(void* args)
{
	struct slave_arg* arg;
	char* mess;
	char* file_id;
	char* part_id;
	int i = 0;
	arg = (struct slave_arg*)args;


	file_id = make_number_string(arg->file_id);
	part_id = make_number_string(arg->part_id);
	mess = malloc(PART_SIZE+22);

	sprintf(mess, "%s.%s.%s", file_id, part_id, arg->part);
	//printf("Przesłana wartość i: %d, przesłana cześć: %s, socket: do ktorego wysle: \n", arg->i, arg->part, slave_addresses[arg->i].sock);
	printf("Wysyłam: %s\n", mess);

	for(i = 0; i <= system_lock+1; i++)
		if(bulk_write(slave_addresses[(arg->i)+i].sock, mess, strlen(mess))< 0) ERR("write:");

	return NULL;
}

char*  make_number_string(int k) {
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
	return result;
}

int main(int argc, char** argv) {
	int socketfd;
	int new_flags;

	srand(time(NULL));

	if(argc<3) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	if(argc == 4)
	{
		system_lock = atoi(argv[3]);
	}

	if(sethandler(SIG_IGN,SIGPIPE)) ERR("Seting SIGPIPE:");
	if(sethandler(sigint_handler,SIGINT)) ERR("Seting SIGINT:");
	if (pthread_mutex_init(&lock, NULL) != 0) ERR("Mutex init:");

	slaves_count = get_number_of_slaves(argv[2]);
	if(slaves_count <= system_lock) {
		printf("Zbyt mało slavevów aby system działał!\n");
		exit(EXIT_FAILURE);
	}


	socketfd = bind_inet_socket(atoi(argv[1]),SOCK_STREAM);
	//new_flags = fcntl(socketfd, F_GETFL) | O_NONBLOCK;
	//fcntl(socketfd, F_SETFL, new_flags);

	if((pthread_create(&(upload_listener_tid), NULL, uploader_listener, (void*)socketfd)) < 0)
		ERR("pthread_create:");


	establish_connection_with_slaves(argv[2], socketfd);

	printf("SYSTEM GOTOWY DO UŻYCIA\n");

	pthread_join(upload_listener_tid, NULL);


	if(TEMP_FAILURE_RETRY(close(socketfd))<0)ERR("close");
	free(slave_addresses);
	fprintf(stderr,"Server has terminated.\n");
	return EXIT_SUCCESS;
}