xterm -hold -e ./server_slave 4000 &
xterm -hold -e ./server_slave 5000 &
xterm -hold -e ./server_main 9000 server_slaves_list
valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes ./new_server_main 9000 server_slaves_list
