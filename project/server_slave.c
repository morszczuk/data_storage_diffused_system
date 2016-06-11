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

#define BACKLOG 3

volatile sig_atomic_t do_work=1 ;

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
    printf("tu doszło\n");
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

int main(int argc, char** argv) {
    int fdT, new_fd;
    struct sockaddr_in addr;
	int new_flags;
    socklen_t size;

    if(argc!=2) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	if(sethandler(SIG_IGN,SIGPIPE)) ERR("Seting SIGPIPE:");
	if(sethandler(sigint_handler,SIGINT)) ERR("Seting SIGINT:");

	fdT=bind_inet_socket(atoi(argv[1]),SOCK_STREAM);
	//new_flags = fcntl(fdT, F_GETFL) | O_NONBLOCK;

	fcntl(fdT, F_SETFL, new_flags);
    while(1)
    {
		printf("CZEKAM NA POŁĄCZENIE\n");
        size = sizeof(struct sockaddr_in);
        if ((new_fd = accept(fdT, (struct sockaddr*)&addr, &size)) == -1)
            ERR("accept");
        printf("SERVER SLAVE: GOT CONNECTION\n");
        if(TEMP_FAILURE_RETRY(close(new_fd))<0)ERR("close");
    }


	//doServer(0,fdT, 0);
	if(TEMP_FAILURE_RETRY(close(fdT))<0)ERR("close");

	fprintf(stderr,"Server has terminated.\n");
	return EXIT_SUCCESS;
}
