all: server_main server_slave client_upload
server_main: server_main.c
	gcc -pthread -Wall -o server_main server_main.c
server_slave: server_slave.c
	gcc -pthread -Wall -o server_slave server_slave.c
client_upload: client_upload.c
	gcc -pthread -Wall -o client_upload client_upload.c
.PHONY: clean
clean:
	rm server_main server_slave
