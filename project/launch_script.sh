xterm -hold -e ./server_slave 4000 &
xterm -hold -e ./server_slave 6000 &
xterm -hold -e ./server_slave 8000 & 
xterm -hold -e ./server_main 9000 server_slaves_list

