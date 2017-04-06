xterm -hold -e valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes --track-origins=yes ./new_server_main 9000 server_slaves_list 3 &
xterm -hold -e valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes --track-origins=yes ./new_server_slave 4000 &
xterm -hold -e valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes --track-origins=yes ./new_server_slave 5000 &
xterm -hold -e valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes --track-origins=yes ./new_server_slave 6000 &
xterm -hold -e valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes --track-origins=yes ./new_client_upload localhost 9000 moj_plik &
xterm -hold -e valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes --track-origins=yes ./administration_client localhost 9000 -s
