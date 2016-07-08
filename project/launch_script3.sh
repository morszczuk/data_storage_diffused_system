xterm -hold -e ./new_server_main 9000 server_slaves_list &
xterm -hold -e ./new_server_slave 5000 &
xterm -hold -e ./new_server_slave 4000 &
xterm -hold -e ./new_client_upload localhost 9000 plik
