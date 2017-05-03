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
#include <assert.h>

volatile sig_atomic_t sm_do_work=1;
volatile sig_atomic_t uc_do_work = 1;
int system_ready = 0;
pthread_mutex_t lock_lock;

void usage(char* name){
    fprintf(stderr,"USAGE: %s port slaves_list_file system_reliability size_part\n",name);
}

void usage_ac(char* name){
    fprintf(stderr,"USAGE: %s address port [mode[-s: status/-n: add new node/-d: delete node [node_id]]\n",name);
}

void sigint_handler(int sig) {
	sm_do_work=0;
}

void sigint_handler_uc(int sig) {
	uc_do_work=0;
}

int sethandler( void (*f)(int), int sigNo) {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if (-1==sigaction(sigNo, &act, NULL))
		return -1;
	return 0;
}

int mkpath(char* file_path, mode_t mode) {
  assert(file_path && *file_path);
  char* p;
  for (p=strchr(file_path+1, '/'); p; p=strchr(p+1, '/')) {
    *p='\0';
    if (mkdir(file_path, mode)==-1) {
      if (errno!=EEXIST) { *p='/'; return -1; }
    }
    *p='/';
  }
  return 0;
}
