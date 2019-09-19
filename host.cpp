#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>

#include <string>
#include <fstream>
#include <iostream>
#include <limits.h>

#include <chrono>
#include <future>
#include <sstream>

#include <iterator>
#include <map>

#define MAXBUFLEN   2048
 // hardcoded for now...

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main (int argc, char* argv[]){  
  
    // -----------------------------------------------------------
    //                  IMPORTANT NODE VARIABLES
    // -----------------------------------------------------------


    //char*       nodename = argv[1];    // NAME OF NODE
    std::string nodeport = "10010";       // PORT OF NODE



    // -----------------------------------------------------------
    //                  SET UP LISTENING SOCKET

    // -----------------------------------------------------------

    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    ssize_t numbytes;
    //struct sockaddr_storage their_addr;
    //socklen_t addr_len;
    //char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, nodeport.c_str(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                            p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        // allow others to reuse the address
        int yes = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            return 1;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }



    freeaddrinfo(servinfo);




    // -----------------------------------------------------------
    //                    SEND MESSAGE:
    // -----------------------------------------------------------

    if ((rv = getaddrinfo("localhost", argv[1], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and make a socket(unused?) -> get address
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if (socket(p->ai_family, p->ai_socktype,
                    p->ai_protocol) == -1) {
            perror("talker: socket");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }
    std::string Src_Router = argv[1];
    std::string destname = argv[2];
    std::string data = argv[3];

    int length = data.length();

    std::string length_msg = std::to_string(length);


    std::string msg = "Type:DATA\nSource_ID: Host\nSource_router:" + Src_Router + "\nLength: " + length_msg + "\nDest_Node:" + destname + "\n"+  data;  
    std::cout << "sent:\n" << msg << std::endl;

    if ((numbytes = sendto(sockfd, msg.c_str(), msg.length(), 0,
                            p->ai_addr, p->ai_addrlen)) == -1) {
        perror("talker: sendto");
        exit(1);
    }
    return 0;
}


