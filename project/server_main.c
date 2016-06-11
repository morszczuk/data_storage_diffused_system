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
#define PART_SIZE 5

volatile sig_atomic_t do_work=1 ;
void* connect_to_slave(void* args);
void* upload_listener(void* args);
void* send_part_of_file_to_slaves(void* args);
int get_number_of_slaves(char* filename);
pthread_t upload_listener_tid;
int slaves_connected = 0;
struct slave* slave_addresses;
int slaves_count;

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

int bind_local_socket(char *name){
	struct sockaddr_un addr;
	int socketfd;
        if(unlink(name) <0&&errno!=ENOENT) ERR("unlink");
	socketfd = make_socket(PF_UNIX,SOCK_STREAM);
	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path,name,sizeof(addr.sun_path)-1);
	if(bind(socketfd,(struct sockaddr*) &addr,SUN_LEN(&addr)) < 0)  ERR("bind");
	if(listen(socketfd, BACKLOG) < 0) ERR("listen");
	return socketfd;
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
	fprintf(stderr,"USAGE: %s port\n",name);
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

void calculate(int32_t data[5]){
	int32_t op1,op2,result,status=1;
	op1=ntohl(data[0]);
	op2=ntohl(data[1]);
	switch((char)ntohl(data[3])){
		case '+':result=op1+op2;
		break;
		case '-':result=op1-op2;
		break;
		case '*':result=op1*op2;
		break;
		case '/':if(!op2) status=0 ;
		    else result=op1/op2;
		    break;
		default: status=0;
	}
	data[4]=htonl(status);
	data[2]=htonl(result);
}

void communicateStream(int cfd){
	ssize_t size;
	int32_t data[5];
	if((size=bulk_read(cfd,(char *)data,sizeof(int32_t[5])))<0) ERR("read:");
	if(size==(int)sizeof(int32_t[5])){
		calculate(data);
		if(bulk_write(cfd,(char *)data,sizeof(int32_t[5]))<0&&errno!=EPIPE) ERR("write:");
	}
	if(TEMP_FAILURE_RETRY(close(cfd))<0)ERR("close");
}

void communicateDgram(int fd){
	struct sockaddr_in addr;
	int32_t data[5];
	socklen_t size=sizeof(addr);;
	if(TEMP_FAILURE_RETRY(recvfrom(fd,(char *)data,sizeof(int32_t[5]),0,&addr,&size))<0) ERR("read:");
	calculate(data);
	if(TEMP_FAILURE_RETRY(sendto(fd,(char *)data,sizeof(int32_t[5]),0,&addr,sizeof(addr)))<0&&errno!=EPIPE) ERR("write:");
}

void doServer(int fdL, int fdT, int fdU){
	int cfd,fdmax;
	fd_set base_rfds, rfds ;
	sigset_t mask, oldmask;
	FD_ZERO(&base_rfds);
	FD_SET(fdL, &base_rfds);
	FD_SET(fdT, &base_rfds);
	FD_SET(fdU, &base_rfds);
	fdmax=(fdT>fdL?fdT:fdL);
	fdmax=(fdU>fdmax?fdU:fdmax);
	sigemptyset (&mask);
	sigaddset (&mask, SIGINT);
	sigprocmask (SIG_BLOCK, &mask, &oldmask);
	while(do_work){
		rfds=base_rfds;
		cfd=-1;
		if(pselect(fdmax+1,&rfds,NULL,NULL,NULL,&oldmask)>0){
			if(FD_ISSET(fdL,&rfds)){
				cfd=add_new_client(fdL);
				if(cfd>=0)communicateStream(cfd);
			}
			if(FD_ISSET(fdT,&rfds)){
				cfd=add_new_client(fdT);
				if(cfd>=0)communicateStream(cfd);
			}
			if(FD_ISSET(fdU,&rfds))
				communicateDgram(fdU);

		}else{
			if(EINTR==errno) continue;
			ERR("pselect");
		}
	}
	sigprocmask (SIG_UNBLOCK, &mask, NULL);
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
	  	address = strtok (line," ");
		port = strtok (NULL, " ");
		printf("Connecting to: %s %s", address, port);
		slave_addresses[i].port = port;
		slave_addresses[i].addr = address;
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

	printf("CONNECTED PORT: %s", (*slave_addr).port);

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

void listen_for_uploader(int socketfd) {


	printf("CHECK 6\n");
}

void* upload_listener(void* args) {
	int are_slaves_connected;
	char* mess_waiting;
	char* mess_ready;
	char buff[BUFF_SIZE];
	int new_fd;
	int fd, c;
	int number_of_parts;
	pthread_t* tids;

	fd = (int)args;


	new_fd = add_new_client(fd);
	mess_waiting = "0";
	mess_ready = "1";
	printf("Mam połączenie z uploaderem\n");
	pthread_mutex_lock(&lock);
	are_slaves_connected = slaves_connected;
	pthread_mutex_unlock(&lock);


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

	while(number_of_parts > 0) {
		if((pthread_create(&(tids[i-1]), NULL, send_part_of_file_to_slaves, (void*)&slave_addresses[i])) < 0)
			ERR("pthread_create:");
		memset(buff,0,sizeof(buff));
		//printf("Waiting for contact from uploader\n");
		//bulk_write(new_fd, mess, strlen(mess));
		c=TEMP_FAILURE_RETRY(read(new_fd,&buff,PART_SIZE));
		if(c < 0)
			ERR("read");
		else {
			printf("%s\n", &buff);
		}
		number_of_parts--;
	}

	if(bulk_write(new_fd, mess_ready, strlen(mess_ready))< 0) ERR("write:");

	return 0;
}

int main(int argc, char** argv) {
	int socketfd;
	int new_flags;
	if(argc!=3) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	if(sethandler(SIG_IGN,SIGPIPE)) ERR("Seting SIGPIPE:");
	if(sethandler(sigint_handler,SIGINT)) ERR("Seting SIGINT:");
	if (pthread_mutex_init(&lock, NULL) != 0) ERR("Mutex init:");

	slaves_count = get_number_of_slaves(argv[2]);

	socketfd = bind_inet_socket(atoi(argv[1]),SOCK_STREAM);
	//new_flags = fcntl(socketfd, F_GETFL) | O_NONBLOCK;
	fcntl(socketfd, F_SETFL, new_flags);

	if((pthread_create(&(upload_listener_tid), NULL, upload_listener, (void*)socketfd)) < 0)
		ERR("pthread_create:");
	printf("CHECK 6\n");
	establish_connection_with_slaves(argv[2], socketfd);
printf("CHECK 7\n");
	printf("SYSTEM GOTOWY DO UŻYCIA\n");

	pthread_join(upload_listener_tid, NULL);

	//doServer(0,fdT, 0);
	if(TEMP_FAILURE_RETRY(close(socketfd))<0)ERR("close");

	free(slave_addresses);
	fprintf(stderr,"Server has terminated.\n");
	return EXIT_SUCCESS;
}
