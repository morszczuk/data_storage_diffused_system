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

#include "utils.h"
#include "connection.h"
#include "read_message.h"

void wait_for_system_readiness_ac(int fd);
void read_status_messages(int fd);

void wait_for_system_readiness_ac(int fd) {
    int system_ready = 0;
    struct message* mess;

    mess = malloc(sizeof(struct message));

    printf("Czekam na gotowość systemu\n");
    while(!system_ready){
        read_mess(fd, mess);
        system_ready = atoi(mess -> message);
        free(mess -> message);
    }

    printf("SYSTEM GOTOWY!\n");

    free(mess);
}

void read_status_messages(int fd) {
    struct message* mess;
    mess = malloc(sizeof(struct message));

    while(1){
        read_mess(fd, mess);
        //printf("")
    }
}

int main(int argc, char *argv[]) {
    int fd, slave_id;
    char* type;

    if(argc < 4) {
        usage_ac(argv[0]);
        return EXIT_FAILURE;
    }

    if(strcmp(argv[3], "-d") == 0){
        if(argc != 5) {
            usage_ac(argv[0]);
            return EXIT_FAILURE;
        } else {
            type = "DEL";
            slave_id = atoi(argv[4]);
        }
    }else if(strcmp(argv[3], "-s") == 0){
        type = "SLV";
    } else if(strcmp(argv[3], "-n") == 0){
        type = "ADM";
    }

    printf("Type: %d\n", *type);

    fd = connect_socket(argv[1], atoi(argv[2]));

	send_message(fd, type, "");

    wait_for_system_readiness_ac(fd);

    if(strcmp(type, "ADM") == 0)
        read_status_messages(fd);

        /*
    switch(*type){
        case 84:
            read_status_messages(fd);
			/*
			HANDLING STATUS ADMINISTRATION CLIENT

		break;
		case 68:
			/*
			HANDLING DELETING NODE ADMINISTRATION CLIENT

		break;
		case 83:
			/*
			HANDLING ADDING NODE ADMINISTRATION CLIENT

		break;
    }*/

    if(TEMP_FAILURE_RETRY(close(fd))<0) ERR("close");

    return 0;
}
