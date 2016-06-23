all: server_main server_slave client_upload new_server_main new_server_slave
server_main: server_main.c
	gcc -pthread -Wall -o server_main server_main.c
server_slave: server_slave.c
	gcc -pthread -Wall -o server_slave server_slave.c
client_upload: client_upload.c
	gcc -pthread -Wall -o client_upload client_upload.c
new_server_main: new_server_main.c
	gcc -pthread -Wall -o new_server_main new_server_main.c
new_server_slave: new_server_slave.c
	gcc -pthread -Wall -o new_server_slave new_server_slave.c
.PHONY: clean
clean:
	rm server_main server_slave
