all: list list_test connection sm_slaves sm_upload files_management utils new_server_main new_server_slave new_client_upload administration_client
list: list.c
	gcc -pthread -Wall -c list.c
list_test: list_test.c list.c
	gcc -pthread -Wall -g -o list_test list_test.c list.c
connection_utils: connection_utils.c
	gcc -pthread -Wall -c connection_utils.c
connection: connection.c
	gcc -pthread -Wall -c connection.c
sm_slaves: sm_slaves.c
	gcc -pthread -Wall -c sm_slaves.c
sm_upload: sm_upload.c
	gcc -pthread -Wall -c sm_upload.c
files_management: files_management.c
	gcc -pthread -Wall -c files_management.c
utils: utils.c
	gcc -pthread -Wall -c utils.c
read_message: read_message.c
	gcc -pthread -Wall -c read_message.c
statuses_list: statuses_list.c
	gcc -pthread -Wall -c statuses_list.c
html_client: html_client.c
	gcc -pthread -Wall -c html_client.c
new_server_main: new_server_main.c connection_utils.c list.c connection.c sm_slaves.c sm_upload.c utils.c files_management.c read_message.c statuses_list.c html_client.c
	gcc -pthread -Wall -o new_server_main new_server_main.c connection_utils.c list.c connection.c sm_slaves.c sm_upload.c utils.c files_management.c read_message.c statuses_list.c html_client.c
new_server_slave: new_server_slave.c utils.c read_message.c
	gcc -pthread -Wall -o new_server_slave new_server_slave.c utils.c read_message.c
new_client_upload: new_client_upload.c
	gcc -pthread -Wall -o new_client_upload new_client_upload.c
administration_client: administration_client.c utils.c connection_utils.c list.h connection.c files_management.c read_message.c
	gcc -pthread -Wall -o administration_client administration_client.c utils.c list.h connection_utils.c connection.c files_management.c read_message.c
.PHONY: clean
clean:
	rm server_main server_slave
