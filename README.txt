This project contains a singular router file "my-router.cpp". It is initiated by passind in the filename and a char of the name of the router. Eg ./my-router A (This will start router A). However the shell script will launch 6 routers/Nodes with names from A to F. To create the shell file, type "chmod +x shell.sh" and then when you want to run the shell script, type in: "sh shell.sh" in the terminal window.

Host.cpp file will send a packet into a Node which will be forwarded to another node/router. In order to send a packet message, enter into the commandline: ./host [destination port] [Char of Node name] [Packet message], for example "./host 10000 F Hello_World". This will send a packet message into Node A with the message "Hello_World" which will then be forwarded onto the Router F through the network 


