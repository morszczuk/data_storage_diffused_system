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

#include "connection.h"

int send_message(int fd, char* mess_type, char* mess) {
	int size;
	char* message;
	int c = 0;
	size = strlen(mess);

	message = malloc(strlen(mess_type) + 10 + size + 3);
	sprintf(message, "%s+%10d%s", mess_type, size, mess);

	c = bulk_write(fd, message, strlen(message));
	free(message);
	return c;
}

void* read_message_sm(int fd){
	char *header_buff, *message_buffer, *mess_type, *mess_size_s;
	int mess_size, c = 0;
	void* return_value;
	struct file_info *file_info;
	struct part_info *part_info;

    header_buff = malloc(HEADER_SIZE+1);
	//mess_size_s = malloc(11);

  while((c = read(fd, header_buff, HEADER_SIZE)) < 0) {
      if(c < 0)
      	ERR("read");
  }

	header_buff[HEADER_SIZE] ='\0';
	mess_type = strtok(header_buff, "+");
	mess_size = atoi(strtok(NULL, "+"));
	message_buffer = malloc(mess_size+1);

	if((c = bulk_read(fd, message_buffer, mess_size)) != mess_size) {
        if(c < 0)
        	ERR("read");
    }

	message_buffer[mess_size] = '\0';

	printf("OTRZYMANA WIADOMOŚĆ: %s\n", message_buffer);

    switch(*mess_type) {
		case 70:
			file_info = malloc(sizeof(struct file_info));
			read_file_info_from_mess(file_info, message_buffer);
			return_value = (void*)file_info;
		break;
		case 80:
			part_info = read_part_info_from_mess(message_buffer);
			return_value = (void*)part_info;
		break;
	}

	free(header_buff);
	free(message_buffer);
	//free(mess_type);
	//free(mess_size_s);

    return return_value;
}
