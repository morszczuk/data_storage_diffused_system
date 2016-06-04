all: server_main server_slave
server_main: server_main.c
	gcc -Wall -o server_main server_main.c
server_slave: server_slave.c
	gcc -Wall -o server_slave server_slave.c
.PHONY: clean
clean:
	rm server_main server_slave
