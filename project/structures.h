#ifndef STRUCTURES_H
#define STRUCTURES_H

//structures
struct connection_arg {
    char* file;
    int sock;
	struct node* slaves_list;
	int part_size;
    pthread_mutex_t* lock;
};

struct slave_node {
	int slave_id;
	int port;
	char* addr;
	int sock;
	pthread_t connection_tid;
	struct slave_node* next;
	int part_size;
  int* active;
};

struct client_thread_arg{
	pthread_t tid;
	int new_fd;
	int fd;
	int part_size;
	struct file_info* files_list;
    pthread_mutex_t* lock;
    struct node* slaves;
    int system_reliability;
    char* type;
};

struct part_info {
    int file_id;
    int part_id;
    char* data;
};

struct get_file_part_arg {
  struct part_info* part;
  struct node* slaves;
};

struct node {
    int id;
    struct node* next;
    void* data;
};

struct file_info {
	int file_id;
	char* filename;
	int file_size;
	int num_of_parts;
	struct file_info* next;
    struct node* parts;
};

struct handle_part_arg{
    struct part_info* part;
    struct node* slaves;
    int system_reliability;
};

struct message{
    char* type;
    char* message;
};

#endif
