# Distance-Vector-Network-Program
This program uses the Bellman Ford Algorthim to send a node down the most efficient path.
This project was completed for 3D3 Computer Networks Module as part of the MAI in Computer and Electronic Engineering in Trinity College Dublin
This program uses the Bellman Ford Algorthim to navigate through a network and adapt to the network if a Node has been removed or an additional router node is added to the network.
The program can be initalised using the Shell Script which will activate 6 routers A-F.

This project contains a singular router file "my-router.cpp". It is initiated by passind in the filename and a char of the name of the router. Eg ./my-router A (This will start router A). However the shell script will launch 6 routers/Nodes with names from A to F. To create the shell file, type "chmod +x shell.sh" and then when you want to run the shell script, type in: "sh shell.sh" in the terminal window.

Host.cpp file will send a packet into a Node which will be forwarded to another node/router. In order to send a packet message, enter into the commandline: ./host [destination port] [Char of Node name] [Packet message], for example "./host 10000 F Hello_World". This will send a packet message into Node A with the message "Hello_World" which will then be forwarded onto the Router F through the network 

