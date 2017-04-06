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

#include "read_message.h"

void read_first_client_mess(int fd, struct message* mess){
	char *header_buff, *message_buffer, *mess_size_s, *type;
	int mess_size, c = 0;

	type = malloc(4);
	printf("read_first_client_mess 1\n");
	while((c = bulk_read(fd, type, 3)) < 3) {
		if(c < 0)
        	ERR("read");
	}

	type[3] = '\0';
	mess -> type = type;
	printf("read_first_client_mess 2\n");
	if(strcmp(mess -> type, "GET") == 0)
		return;

	printf("read_first_client_mess 3\n");

    header_buff = malloc(HEADER_SIZE);

	printf("read_first_client_mess 4, MESS_TYPE: %s\n", mess -> type);
    while((c = bulk_read(fd, header_buff, HEADER_SIZE-1)) < 0) {
        if(c < 0)
        	ERR("read");
    }

	printf("read_first_client_mess 5\n");
	header_buff[HEADER_SIZE-1] ='\0';
	printf("Przed atoi\n");
	mess_size = atoi(strtok(header_buff, "+"));
	printf("OTRZYMANY MESS_SIZE: %d\n", mess_size);
	message_buffer = malloc(mess_size+1);

	if((c = bulk_read(fd, message_buffer, mess_size)) != mess_size) {
        if(c < 0)
        	ERR("read");
    }

	message_buffer[mess_size] = '\0';

	printf("OTRZYMANA WIADOMOŚĆ: %s\n", message_buffer);

	mess -> message = message_buffer;

	free(header_buff);

}


void read_mess(int fd, struct message* mess){
	char *header_buff, *message_buffer, *mess_type;
	int mess_size, c = 0;

    header_buff = malloc(HEADER_SIZE+1);

    while((c = read(fd, header_buff, HEADER_SIZE)) < HEADER_SIZE) {
        if(c < 0)
        	ERR("read");
    }

	header_buff[HEADER_SIZE] ='\0';
	printf("OTRZYMANA NAGŁÓWEK: %s\n", header_buff);
	mess_type = strtok(header_buff, "+");
	mess_size = atoi(strtok(NULL, "+"));
	message_buffer = malloc(mess_size+1);

	if((c = bulk_read(fd, message_buffer, mess_size)) != mess_size) {
        if(c < 0)
        	ERR("read");
    }

	message_buffer[mess_size] = '\0';

	printf("OTRZYMANA WIADOMOŚĆ: %s\n", message_buffer);

	mess -> message = message_buffer;
    mess -> type = mess_type;

	free(header_buff);
}
