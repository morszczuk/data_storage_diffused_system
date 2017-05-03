xterm -hold -e ../new_server_main 9000 example_slaves_list 3 &
xterm -hold -e ../new_server_slave 4000 &
xterm -hold -e ../new_server_slave 5000 &
xterm -hold -e ../new_server_slave 6000 &
xterm -hold -e ../new_client_upload localhost 9000 example_file &
xterm -hold -e ../administration_client localhost 9000 -s
