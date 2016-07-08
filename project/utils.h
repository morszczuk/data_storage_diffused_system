#ifndef UTILS_H
#define UTILS_H

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

#include "constants.h"
//#include "structures.h"

extern volatile sig_atomic_t sm_do_work;
extern volatile sig_atomic_t uc_do_work;
extern int system_ready;
extern pthread_mutex_t lock_lock;

void usage(char* name);
void sigint_handler(int sig);
void sigint_handler_uc(int sig);
int sethandler( void (*f)(int), int sigNo);
void wait_for_system_readiness(int fd, pthread_mutex_t* lock);
int mkpath(char* file_path, mode_t mode);

#endif
