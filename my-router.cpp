//
//  my-router.cpp
//  
//
//  Created by Sean Murray on 02/04/2019.
//

// changed the int port number to char*
// in order to have a proper input to the address function

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
#include <time.h> 
#include <cstdbool>
#include <ctime>
#include <cstdlib>
#include <string>
#include <fstream>
#include <iostream>
#include <limits.h>

#include <chrono>
#include <future>
#include <sstream>
#include <thread>
#include <pthread.h>
#include <iterator>
#include <map>
#include <cmath>


#define MAXBUFLEN   2048

const int NEIGHBOUR_UPDATE_TIMEOUT = 8; // if no recv for 5s, ping neighbours
const int DEATH_TIMEOUT = 25;


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa);


// MAPS + STRUCTS for tables

typedef std::map<char, int> DV_MAP;

struct
neighbour
{
    int cost;
    char *port;
    DV_MAP distancevectors;
};

typedef std::map<char, bool>        D_MAP;       //<neighbour name, sent death message>
typedef std::map<char, neighbour>   N_MAP;      // <name, node>
typedef std::map<char, char*>       FT_MAP;     // <finaldest, nextport>
typedef std::pair<char, neighbour>  N_PAIR;     // <name, node>
typedef std::pair<char, int>        DV_PAIR;    // <finaldest, totalcost>
typedef std::pair<char, char*>      FT_PAIR;    // <finaldest, nextport>
typedef std::map<char, time_t>         TS_MAP;     //<final dest, last time checked> 
typedef std::pair<char, time_t>        TS_PAIR;     //<final dest, last time checked> 
typedef std::pair<char,bool>        D_PAIR;          //<neighbour name, sent death message>

bool bellmanford(N_MAP ntable, DV_MAP *currDV, FT_MAP *ftable);
bool dvupdate   (char thisnode, char nodeX, DV_MAP newtable, N_MAP *ntable, DV_MAP *owntable, FT_MAP *ftable, TS_MAP * timeStamp,N_MAP history);
//bool is_file_exist(std::string fileName);

std::string dvtostring (DV_MAP newtable, std::string nodename);
DV_MAP      stringtodv (std::string message, char* srcnode);
void reinitDV(N_MAP *ntable, DV_MAP *currDV, D_MAP* dAcks);


time_t my_time = time(NULL);

time_t timeSeconds;             // time struct used to store current time
TS_MAP  timeStamp;             //The time stamp book       
char deadNode = '\n';           // name of node to be removed
char storage = deadNode;
bool nodeRemoved = true;        // Bool blocking adding of new nodes until old node is removed
bool deathAcked = true;
int main(int argc, char *argv[])
// eg
// ./server [myport#] [destport#] - will be ./server [name]

{
    assert(argc > 1); // ensure program called w/ arguments

    // -----------------------------------------------------------
    //                  IMPORTANT NODE VARIABLES
    // -----------------------------------------------------------
    
    
    char    *nodename = argv[1];    // NAME OF NODE
    char    *nodeport = NULL;       // PORT OF NODE
    DV_MAP  nodeDVs;                // NODE'S DISTANCE VECTOR TABLE <dests, distances>
    FT_MAP  nodeFT;                 // NODE'S FORWARDING TABLE <dests, nextports>
    N_MAP   neighbourtable;         // NODE'S NEIGHBOUR INFO <neighb, neighb-info>
    D_MAP   deathAcks;              //
    N_MAP   neighbourHistory;       // REPRESENTS NODES THAT ARE HARDLINKED TO THIS NODE
    // -----------------------------------------------------------
    //                      INITIALISATION
    // -----------------------------------------------------------


    std::ifstream inFile("graph.csv",std::ios::in);
    
    if(!inFile) { std::cout<< "Error:infile"; exit(EXIT_FAILURE);}
    
    std::string buffer;
    std::string source;
    std::string dest;
    char* destPort;
    int   linkCost;
    
    bool flag = 1;

    
    while(inFile.good()){
        
        getline(inFile, source, ',');
        getline(inFile, dest,',');
        getline(inFile, buffer,',');
        destPort = strdup(buffer.c_str());
        getline(inFile, buffer,'\n');
        linkCost = stoi(buffer);
        
        if(source == nodename){
            
            // insert to forward table
            nodeFT.insert(FT_PAIR(dest[0], destPort));
            
            // insert to DVs table
            nodeDVs.insert(DV_PAIR(dest[0], linkCost));
            
            // make neighbour node, and insert it to neighbour table
            neighbour n;
            n.cost = linkCost;
            n.port = destPort;
            //n.distancevectors = NULL;
            neighbourtable.insert(N_PAIR(dest[0], n));
            neighbourHistory.insert(N_PAIR(dest[0], n));
            deathAcks.insert(D_PAIR(dest[0],false));


            //Add the node to the time stamp book and mark it's first time;
            
            timeSeconds = time(NULL);
        
            std::cout << "The timeSeconds is " << timeSeconds << "\n";
            timeStamp.insert(TS_PAIR(dest[0],timeSeconds));

            
            
        }
        
        if(dest == nodename && flag) {// this is our portneighbourtable.insert(N_PAIR(dest[0], n));
            
            nodeport = destPort;
            std::cout << "\nFound nodeport: " << nodeport << std::endl;
            flag = 0;
        }
        //free(destPort);
    }
    
    assert(nodeport!=NULL); // ensure nodeport is initialised


    // open up write file

   // std::string src(1, nodename);

    std::string fileName = "";
                fileName +="routing-output";
                fileName += nodename;
                fileName += ".txt";
    

    // std::string src(1, nodename);
    // std::string newFileName=" ";
    //             newFileName +="newFile";
    //             newFileName +=src;

    std::fstream newFileName;

    newFileName.open(fileName, std::ios::out);
    

    std::cout << "file is now open.. now writing...\n";
                

    
    // -----------------------------------------------------------
    //                 PRINT INITIAL NODE STATE
    // -----------------------------------------------------------
    
     // printing neighbours
    N_MAP::iterator itrN; 
    FT_MAP::iterator itrFT;

    //We only need one table intially starting off

    //--------------------------
    //     Print to Terminal
    //--------------------------
    std::cout<<std::endl << "\nInitial Forwarding Table for " << nodename << ": \n\n";
    std::cout <<"-------------------------------------------------------------------"  <<std::endl;
    std::cout<<std::endl << "Destination | \tCost | \tOutgoing UDP Port | Destination UDP Port |\n";
    for (itrN = neighbourtable.begin(); itrN != neighbourtable.end(); ++itrN) {
        std::cout<<std::endl << '\t' <<  itrN->first
        << '\t' << "  " << itrN->second.cost << '\t'<<  nodeport << "( Node " << nodename << " )" <<'\t'<< '\t' << itrN->second.port << "( Node " << itrN->first << " )" << '\t';
    }
    std::cout << std::endl << std::endl <<"-------------------------------------------------------------------" <<std::endl;
    //--------------------------
    //      Print to File
    //--------------------------
    newFileName<<std::endl << "\nInitial Forwarding Table for " << nodename << ": \n\n";
    newFileName <<"-------------------------------------------------------------------"  <<std::endl;
    newFileName<<std::endl << "Destination | \tCost | \tOutgoing UDP Port | Destination UDP Port |\n";
    for (itrN = neighbourtable.begin(); itrN != neighbourtable.end(); ++itrN) {
        newFileName<<std::endl << '\t' << '\t' << itrN->first
        << '\t' << '\t' <<   itrN->second.cost << '\t'<< '\t'<<  nodeport << "( Node " << nodename << " )" <<'\t'<< '\t' << itrN->second.port << "( Node " << itrN->first << " )" << '\t';
    }
    newFileName<< std::endl << std::endl <<"-------------------------------------------------------------------" <<std::endl;
    
    
    // -----------------------------------------------------------
    //                  SET UP LISTENING SOCKET
    // -----------------------------------------------------------
    
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    ssize_t numbytes;
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len;

    std::string msg;
    std::string srcname;

    //char s[INET6_ADDRSTRLEN];
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    
    if ((rv = getaddrinfo(NULL, nodeport, &hints, &servinfo)) != 0) {
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
    
    
    // *** NOT SURE ABOUT FREEING ADDRINFO HERE...
    freeaddrinfo(servinfo);
    
    // -----------------------------------------------------------
    //                  BEGIN LISTENING FOR() LOOP
    // -----------------------------------------------------------
    
    addr_len = sizeof their_addr;
    
    // set up future with recvfrom() function
    std::future<ssize_t> fut = std::async(recvfrom, sockfd, buf, MAXBUFLEN-1 , 0,
                                          (struct sockaddr *)&their_addr, &addr_len);
    
    std::chrono::seconds span (NEIGHBOUR_UPDATE_TIMEOUT);

    
    for(;;) {

        //std:: cout << "Currentl DeathAcked is : " << deathAcked<< "\n";
        std::cout<<std::endl << std::endl << nodename << ": waiting to recvfrom...\n";

           // -----------------------------------------------------------
        //                  SET UP NODE DEATH TIMERS
        // -----------------------------------------------------------
        
        //We have a time stamp map, when a node is initialized or otherwise added to the NodeDV table
        // we make a time stamp in the timeStamp map.
        // Each time we recieve communication from a node their entry in the map is stamped again
        // At the top of the listeing loop we check to see if any entry was stamped mroe than 60 seconds ago
        // If so then we declare the node dead, send out a DV table with that nodes entry as a negative link cost 
        // Then we remove that node from all our table 
        
        timeSeconds =  time(NULL);
        TS_MAP::iterator itrTS;
        for(itrTS = timeStamp.begin(); itrTS!= timeStamp.end(); itrTS++){

            //std::cout<< itrTS->first  << " Node, Time Stamp : " << timeSeconds - itrTS->second  << "=====================\n";   
            if(timeSeconds - itrTS->second >DEATH_TIMEOUT){
                //std::cout << "The time for node" << itrTS->first << " has exceeded "<< DEATH_TIMEOUT <<" seconds\n";
                deadNode = itrTS->first;
                //do stuff
                DV_MAP deathSignal = nodeDVs;
                (deathSignal.find(deadNode))->second = 9999999;

                //Do stuff 
                N_MAP::iterator itrN;
                neighbourtable.erase(deadNode);
                deathAcks.erase(deadNode);
                
                std::cout << "Alerting Neighbours to Dead Node" << std::endl;
                msg = dvtostring(deathSignal,nodename);

                
                for (itrN = neighbourtable.begin(); itrN != neighbourtable.end(); itrN++ ) {
                
                    
                    if ((rv = getaddrinfo("localhost", itrN->second.port, &hints, &servinfo)) != 0) {
                        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                        return 1;
                    }
                    // loop through all the results and make a socket(unused?) -> get address
                    for(p = servinfo; p != NULL; p = p->ai_next) {
                        if (socket(p->ai_family, p->ai_socktype,
                                p->ai_protocol) == -1) {
                            perror("socket");
                            continue;
                        }
                        break;
                    }
                
                    if (p == NULL) {
                        fprintf(stderr, "failed to create socket\n");
                        return 2;
                    }
                
                    if ((numbytes = sendto(sockfd, msg.c_str(), msg.length(), 0,
                                        p->ai_addr, p->ai_addrlen)) == -1) {
                        perror("sendto");
                        exit(1);
                    }
                    
                    std::cout << " Sent Death message to " << itrN->first << "+++\n" ;
                    
                } // updated all neighbours
                
                std::cout<<std::endl;   // << ":\n" << ping_msg << "\n\n";

                std::cout << "Removing Dead Node: " << deadNode <<" from tables." << std::endl;
                
                //neighbourtable.erase(deadNode);
                nodeDVs.erase(deadNode);
                nodeFT.erase(deadNode);
                timeStamp.erase(deadNode);
                deathAcks.clear();
                reinitDV(&neighbourtable, &nodeDVs,&deathAcks);
            }
        }
        
        // -----------------------------------------------------------
        //                      PING/recvfrom() TIMEOUT
        // -----------------------------------------------------------
        
        // 5s timeout
        // exit while() loop when either:
        // 1. recvfrom() returns a message
        // 2. the 5s 'span' is over
        
        // if timespan is over -> no recv in 5s -> ping neighbours
        // if recv -> update: table then ping neighbours
        //         -> datagram: send on, don't update neighbours
        //              (maybe we should ping neighbours after recv dgram?)


        // if...
        // (timeout)https://en.wikipedia.org/wiki/Select_%28Unix%29

        int maxTS = 0;
        long int dynamic_span;

        timeSeconds = time(NULL); //Set timestamp to NULL
        for(itrTS = timeStamp.begin(); itrTS != timeStamp.end(); ++itrTS) { //Cycle through Timestamp Map
            if(timeSeconds - itrTS->second > maxTS) //If the time value minus the current Timestamp value is greater than maxTS (0)
                maxTS = timeSeconds - itrTS->second; //MaxTs is time in seconds minus the timestamp value
        }

        if(maxTS > 0 && maxTS < floor(0.8*DEATH_TIMEOUT))
            dynamic_span = ceil(1000*(/*DEATH_TIMEOUT-*/maxTS)*NEIGHBOUR_UPDATE_TIMEOUT/DEATH_TIMEOUT) + rand()%100;
        else
            dynamic_span = 500 + rand()%50;


        std::chrono::milliseconds span(dynamic_span);


        if(fut.wait_for(span)==std::future_status::timeout)
        {
            // if(ping) {
            // std::cout<<std::endl << nodename << ": recvfrom() timeout! pinging neighbours...\n\n";
            
           // std::cout << "pinged at " << dynamic_span << "ms\n";
            
            // -----------------------------------------------------------
            //              PING NEIGHBOURS WITH CURRENT TABLE:
            // -----------------------------------------------------------
            

            msg = dvtostring(nodeDVs,nodename);
            
            for (itrN = neighbourtable.begin(); itrN != neighbourtable.end(); ++itrN) { //Send to all Neighbours
                
                if ((rv = getaddrinfo("localhost", itrN->second.port, &hints, &servinfo)) != 0) {
                    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                    return 1;
                }
                // loop through all the results and make a socket(unused?) -> get address
                for(p = servinfo; p != NULL; p = p->ai_next) {
                    if (socket(p->ai_family, p->ai_socktype,
                               p->ai_protocol) == -1) {
                        perror("socket");
                        continue;
                    }
                    break;
                }
            
                if (p == NULL) {
                    fprintf(stderr, "failed to create socket\n");
                    return 2;
                }
            
                if ((numbytes = sendto(sockfd, msg.c_str(), msg.length(), 0,
                                       p->ai_addr, p->ai_addrlen)) == -1) {
                    perror("sendto");
                    exit(1);
                }
                
                std::cout << "Sending Update to: " << itrN->first << std::endl;
                
            } // updated all neighbours
            
            std::cout<<std::endl;   // << ":\n" << ping_msg << "\n\n";
            
            continue;
        }
        
        
        
        // else(no timeout)... message received!

        else {
            
            // -----------------------------------------------------------
            //                      MESSAGE RECEIVED
            // -----------------------------------------------------------
            
            //ssize_t
            numbytes = fut.get();
            if (numbytes == -1) { perror("recvfrom"); return 9;}
            
            // eventual objectives on recvfrom():
            //  CTRL - update table, ping neighbours with update
            //  DATA - lookup table, forward packet on
            
           
            
            buf[(int)numbytes] = '\0';
           // std::cout<<std::endl << nodename << ": message: \n" << buf << std::endl << std::endl;
            


            //------------------------------------------------------------
            //                  Parsing the Message for Type
            //------------------------------------------------------------
            
            // Either CTRL (Control message) or DATA (Datagram Message)
            
            //The data that is recived in the buffer gets stored in string recvd_message
            std::string recvd_message = buf;

           
            //Extract the type of message, this removes the "Type:" from the recvd_message
            int position_1 = recvd_message.find(":");
            recvd_message.erase(0,position_1+1);
            
            //In the new code, the find function will search for a new line "\n" 
            int position_2 = recvd_message.find("\n");       
            std::string type_message = recvd_message.substr(0,position_2); //Make a substring called type_message that stores either "CTRL" or "DATA" depending on the packet
            //std::cout<<std::endl << "Type: " << type_message << std::endl; //Print out to the Screen
            
            //Extract the body of the message
            recvd_message = recvd_message.erase(0, position_2+1); //Remove the Type of message header from the string

           // std::cout<<std::endl <<"Type of Message = " << type_message <<std::endl;


            // -----------------------------------------------------------
            //                  CTRL: UPDATE, SEND ON UPDATES
            // -----------------------------------------------------------

            if(type_message == "CTRL"){
                // so we've received an updated DV from our neighbour

                char source;
                //std::string src(1,source);
                DV_MAP recvdDVs;
                
                recvdDVs = stringtodv(recvd_message, &source);
                //std::cout << nodename << ": CTRL packet from " << source << std::endl;
                
                //-----------------------------------------------------------------------------------------------
                //                      This prints out the inital routing table which will not get updated
                //-----------------------------------------------------------------------------------------------
                //Print out inital DV table to file

                DV_MAP::iterator itrDV;
                DV_MAP::iterator itrDv;
              
                    //need to write the out the DV that changed everything
                    // dv update returns 1 if there are changes made
                    // ...to our neighbour table distancevectors

                //Itterate through out incoming DV map so that we 
                //may find a death message before running Bellman ford
                
                D_MAP::iterator itrD;
                for (itrDV = recvdDVs.begin(); itrDV != recvdDVs.end(); ++itrDV) {
                    if(itrDV->second >= 9000000){
                        std::cout<<"There;s a dead one in here \n";
                    }
                }

                //Dealing with Dead Nodes
                for (itrDV = recvdDVs.begin(); itrDV != recvdDVs.end(); ++itrDV) {
                    
                     if((itrDV->second >= 9000000)){
                         std::cout << "Death message recived from " << source << std::endl;
                        deadNode = itrDV->first;
                        deathAcks.erase(deadNode);
                        deathAcked = false;
                        bool allFlag = true;
                        (deathAcks.find(source))->second = true;
                        for (itrD = deathAcks.begin();itrD!=deathAcks.end();++itrD){
                            if(itrD->second == false){
                                allFlag = false;
                                std::cout << "The node " << itrD->first << " Has not acked \n";
                            }
                        }
                        if(allFlag){
                            deathAcked = true;
                            
                            for (itrD = deathAcks.begin();itrD!=deathAcks.end();++itrD){
                                itrD->second = false;
                            }
                            
                        }
                        else{
                            deathAcked = false;
                        }
                        //recvdDVs.erase(deadNode);
                        //std::cout << "deathAcked updated to :" << deathAcked << std::endl;
                        std::cout << "Removed from recdv \n";
                        for (itrDv = recvdDVs.begin(); itrDv != recvdDVs.end(); ++itrDv) {
                        std::cout<< itrDv->first << ":" << itrDv->second << "\n"; 
                    }
                       
                    }
                    if((itrDV->second >= 9999990)&&(nodeDVs.count(deadNode)!=0)){
                        
                        nodeRemoved = false;
                        //recvdDVs.erase(deadNode);
                    }
                    
                }
                //Accepting and Adding Deadnode to DV table
                if(!nodeRemoved){
                    
                    DV_MAP outGoing = recvdDVs;
                    outGoing.insert(DV_PAIR(deadNode,9999999)); //Add the Dead Node to the DV table
                    neighbourtable.erase(deadNode); //Remove from Neighbourtable
                    deathAcks.erase(deadNode); 
                    std::cout << "Alerting Neighbours to Dead Node" << std::endl;
                    msg = dvtostring(outGoing,nodename); //Make a new DV table without the Node

                    //std::cout<<std::endl <<"Message = " << mess <<std::endl;
                    //std::string ping_msg = s + " TYPE:CTRL " + itrN->first + " on port: " + itrN->second.port + '\n';

                    
                    for (itrN = neighbourtable.begin(); itrN != neighbourtable.end(); ++itrN) {
                    
                        //std::string DESTPEER = argv[2];
                        // find DESTPEER port in neighbour table....
                        
                        // std::string ping_msg = nodename + " TYPE:CTRL " + itrN->first + " on port: " + itrN->second.port<<ctime(&my_time) + '\n';
                        std::cout << "Sending death message to neighbour "<< itrN->first << "$$\n";
                        if ((rv = getaddrinfo("localhost", itrN->second.port, &hints, &servinfo)) != 0) {
                            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                            return 1;
                        }
                        // loop through all the results and make a socket(unused?) -> get address
                        for(p = servinfo; p != NULL; p = p->ai_next) {
                            if (socket(p->ai_family, p->ai_socktype,
                                    p->ai_protocol) == -1) {
                                perror("socket");
                                continue;
                            }
                            break;
                        }
                    
                        if (p == NULL) {
                            fprintf(stderr, "failed to create socket\n");
                            return 2;
                        }
                    
                        if ((numbytes = sendto(sockfd, msg.c_str(), msg.length(), 0,
                                            p->ai_addr, p->ai_addrlen)) == -1) {
                            perror("sendto");
                            exit(1);
                        }
                        
                        //std::cout << " " << itrN->first;
                        
                    } // updated all neighbours
                    
                    std::cout<<std::endl;   // << ":\n" << ping_msg << "\n\n";

                    std::cout << "Removing Dead Node: " << deadNode <<" from tables." << std::endl;
                    
                    //neighbourtable.erase(deadNode);
                    nodeDVs.erase(deadNode); //Remove Node from DV table
                    nodeFT.erase(deadNode); //Remove Node from Forwarding Table
                    timeStamp.erase(deadNode); //Remove its timestamp
                    reinitDV(&neighbourtable, &nodeDVs, &deathAcks); // Restart DV and BellmanFord Operation

                    nodeRemoved = true;
                }
                

                
                if(dvupdate(nodename[0], source, recvdDVs, &neighbourtable, &nodeDVs, &nodeFT, &timeStamp, neighbourHistory)) {
                    //
                    
                    std::cout << "table updated! now need to perform bellman-ford...\n";
                    
                    // update own DV & forward table... (bellman-ford)
                   
                    if(bellmanford(neighbourtable, &nodeDVs, &nodeFT)) {

                        // print out our updated DV table
                        
                    //  ----------------------------------------------------------------------
                    //                   Updated DV table for our Router
                    //  -----------------------------------------------------------------------
                        newFileName << "------------------------------------------------------------------"<<std::endl;
                        newFileName <<"                       Updated Tables                              " <<std::endl;
                        newFileName <<"-------------------------------------------------------------------" <<std::endl;
                        newFileName<<ctime(&my_time) << "\nupdated DV table" << ": \n\n";
                        newFileName<<"___________________________________________________" <<std::endl;
                        newFileName<<std::endl << "Destination |";
                        for (itrDV = nodeDVs.begin(); itrDV != nodeDVs.end(); ++itrDV) {
                            newFileName << '\t' <<  itrDV->first;
                            
                            }
                        newFileName << std::endl;
                        newFileName<<std::endl << "Cost        |";
                        for (itrDV = nodeDVs.begin(); itrDV != nodeDVs.end(); ++itrDV) {
                            if(itrDV->second==INT_MAX) newFileName<< '\t' << "\u221E"; // if infinity
                            else newFileName<< '\t'  <<  itrDV->second;
                        }
                        newFileName << std::endl << "___________________________________________________" <<std::endl;
                        newFileName<<std::endl;

                        

                        // Print Forwarding Table in File

                        //FT_MAP::iterator itrFT;
                           

                        newFileName <<"Updated Forwarding Table" <<std::endl;
                        newFileName <<"-------------------------------------------------------------------"  <<std::endl;
                        newFileName<<std::endl << "Destination | \tCost | \tOutgoing UDP Port | Next Hop UDP Port |\n";
                        for (itrFT = nodeFT.begin(); itrFT != nodeFT.end(); ++itrFT) {
                            newFileName<<std::endl << '\t' <<  itrFT->first
                            << '\t' << '\t' << '\t'; 
                            char name = itrFT->first;
                    
                            newFileName << nodeDVs.find(name)->second; 
                    
                            newFileName << '\t' << '\t'  << '\t' << nodeport<< "( Node " << nodename << " )" <<'\t'<< '\t' << itrFT->second << '\t';
                        }
                        newFileName << std::endl << std::endl <<"-------------------------------------------------------------------" <<std::endl;

                        std::cout << std::endl;

                        std::cout <<"Updated Forwarding Table" <<std::endl;
                        std::cout <<"-------------------------------------------------------------------"  <<std::endl;
                        std::cout<<std::endl << "Destination | \tCost | \tOutgoing UDP Port | Next Hop UDP Port |\n";
                        for (itrFT = nodeFT.begin(); itrFT != nodeFT.end(); ++itrFT) {
                            std::cout<<std::endl << '\t' <<  itrFT->first
                            << '\t'; 
                            char name = itrFT->first;
                    
                            std::cout << nodeDVs.find(name)->second; 

                            std::cout << '\t' << nodeport<< "( Node " << nodename << " )" <<'\t'<< '\t' << itrFT->second << '\t';
                        }
                        std::cout << std::endl << std::endl <<"-------------------------------------------------------------------" <<std::endl;




                        // -----------------------------------------------------------
                        //          PING NEIGHBOURS WITH UPDATED TABLE FROM bf:
                        // -----------------------------------------------------------
                        
                        std::cout<<std::endl << "updating neighbours ";
                        

                        msg = dvtostring(nodeDVs,nodename);

                        
                        for (itrN = neighbourtable.begin(); itrN != neighbourtable.end(); ++itrN) {
                        
                            if ((rv = getaddrinfo("localhost", itrN->second.port, &hints, &servinfo)) != 0) {
                                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                                return 1;
                            }
                            // loop through all the results and make a socket(unused?) -> get address
                            for(p = servinfo; p != NULL; p = p->ai_next) {
                                if (socket(p->ai_family, p->ai_socktype,
                                        p->ai_protocol) == -1) {
                                    perror("socket");
                                    continue;
                                }
                                break;
                            }
                        
                            if (p == NULL) {
                                fprintf(stderr, "failed to create socket\n");
                                return 2;
                            }
                        
                            if ((numbytes = sendto(sockfd, msg.c_str(), msg.length(), 0,
                                                p->ai_addr, p->ai_addrlen)) == -1) {
                                perror("sendto");
                                exit(1);
                            }
                            
                            std::cout << " " << itrN->first;
                            
                        } // updated all neighbours
                        
                        //newFileName.close(); 

                    }

                }
            
            } 
            // -----------------------------------------------------------
            //                  DATA: FORWARD MESSAGE ON
            // -----------------------------------------------------------

            // how do we find the address of the node we want to send to?
            //  -> use socket() to get the address of the destination port...
            
            // using hardcoded port: DESTPEER "4951"...
            //  (in future, this port# will be found using DV or lookup table)
            // now use socket()...
            //  -> to get the address we need: [p->ai_addr, p->ai_addrlen]
            

            if(type_message == "DATA"){
                
                // structure:
                // "Type:DATA\n<parsed>
                // Src_Node:"+ srcname +"\nSrc_Node:" + destname +"\n"+ recvd_message;

                //--------------------------------------
                //     Initial Source of packet
                //--------------------------------------
                my_time =  time(NULL);
                newFileName <<std::endl << "Data Packet Recieved " <<ctime(&my_time) << std::endl;
                int position_ctrl_1 = recvd_message.find(":");
                recvd_message.erase(0,position_ctrl_1+1);
                int position_ctrl_2 = recvd_message.find("\n");
                std::string src_id = recvd_message.substr(0,position_ctrl_2);
                recvd_message.erase(0, position_ctrl_2+1);

                //std::string srcname = src_router_name; // char* = string ??? may need to c_str() or index [0]

                newFileName << "Source ID: " << src_id << std::endl;
                std::cout << "Source ID: " << src_id << std::endl;
              
                int position_ctrl_A = recvd_message.find(":");
                recvd_message.erase(0,position_ctrl_A+1);
                int position_ctrl_B = recvd_message.find("\n");
                std::string src_router_name = recvd_message.substr(0,position_ctrl_B);
                recvd_message.erase(0, position_ctrl_B+1);

                std::string srcname = src_router_name; // char* = string ??? may need to c_str() or index [0]

                newFileName << "Source Port: " << srcname << std::endl;
                std::cout << "Source Port: " << srcname <<std::endl;

                int position_ctrl_C = recvd_message.find(":");
                recvd_message.erase(0,position_ctrl_C+1);
                int position_ctrl_D = recvd_message.find("\n");
                std::string length_msg = recvd_message.substr(0,position_ctrl_D);
                recvd_message.erase(0, position_ctrl_D+1);

                newFileName<< "Length:" << length_msg << std::endl;
                std::cout <<"Length:"<<length_msg <<std::endl;

                //--------------------------------------
                //     Final Destination Router Name
                //--------------------------------------
                
                int position_ctrl_E = recvd_message.find(":");
                recvd_message.erase(0,position_ctrl_E+1);
                int position_ctrl_F = recvd_message.find("\n");
                std::string dest_router_name = recvd_message.substr(0,position_ctrl_F);
                recvd_message.erase(0, position_ctrl_F+1);
              
                std::string destname = dest_router_name; // char* = string ??? may need to c_str() or index [0]    
                  if(destname == nodename){
                      std::cout << std::endl << "Message Recieved!"<<std::endl;
                      newFileName << "Message: " << recvd_message <<std::endl;
                      std::cout<< "message:\n" << recvd_message << "\n";  // output what is left of the packet

                  }
                else{
                    std::cout << "Forwarding Message to:" << destname << std::endl;
                    newFileName << "Forwarding Message to:" << destname << std::endl;
                    newFileName<< "Message: " << recvd_message << "\n";  // output what is left of the packet
                    //-----------------------------------------------------------
                    //                  Forwarding
                    //-----------------------------------------------------------
                    // lookup next hop port in the forward table
                    std::string nexthop_port = nodeFT.find(destname[0])->second;

                    newFileName << std::endl << "Forwarding to Node: " << nodeFT.find(destname[0])->first <<std::endl;
                    std::string name = nodename;

                    msg = "Type:DATA\nSource_ID:" + name + "\nSrc Port:" + srcname +"\nLength:" + length_msg + "\nDest_Node:" + destname +"\n" + recvd_message + "\n";
                    std::cout << "Next Hop Port: " << nexthop_port << std::endl <<std::endl;
                   newFileName << "Next Hop port: " << nexthop_port << std::endl << std::endl;
                   newFileName << "Message sent: "  << std::endl << msg << std::endl;

                    
        
                    // create address using port from FT...
                    if ((rv = getaddrinfo("localhost", nexthop_port.c_str(), &hints, &servinfo)) != 0) {
                        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                        return 1;
                    }

                    // loop through all the results and make a socket(unused?) -> get address
                    for(p = servinfo; p != NULL; p = p->ai_next) {
                        if (socket(p->ai_family, p->ai_socktype,
                                            p->ai_protocol) == -1) {
                            perror("socket");
                            continue;                std::cout << std::endl;
                        }
                        break;
                    }
        
                    if (p == NULL) {
                        fprintf(stderr, "failed to create socket\n");
                        return 2;
                    }
        
                    // send msg...
        
                    if ((numbytes = sendto(sockfd, msg.c_str(), msg.length(), 0,
                                                    p->ai_addr, p->ai_addrlen)) == -1) {
                        perror("sendto");
                        exit(1);
                    }
                }
            
            }

            // -----------------------------------------------------------
            //                       DATA: END
            // -----------------------------------------------------------
            
            // recall recvfrom() function...
            
            fut = std::async(recvfrom, sockfd, buf, MAXBUFLEN-1 , 0,
                             (struct sockaddr *)&their_addr, &addr_len);
            
        }
        
        // -----------------------------------------------------------
        //                      END LISTENING FOR LOOP
        // -----------------------------------------------------------
        
    }
    
    
    freeaddrinfo(servinfo);
    close(sockfd);
    
    newFileName.close();
    return 0;

}

bool bellmanford(N_MAP ntable, DV_MAP *currDV, FT_MAP *ftable) {
    

    // go through our DVs, check if there is a better cost found in our neighbour table
    bool bfupdate = false;
    DV_MAP::iterator itrDV;

    for(itrDV = currDV->begin(); itrDV != currDV->end(); ++itrDV) {

        char nodeX = itrDV->first; //This is the current Node we are moving through on our DV table

        // what if some of these are not found?
        
        int currpathcost = currDV->find(nodeX)->second; //Its current path cost from this router

        //std::cout <<"Currently I can get to " << nodeX <<" with a cost of: " << currpathcost << std::endl <<std::endl;
        
        int DvX;    // neighbour distance to nodeX
        int Cv;     // neighbour link cost
        int newpathcost;
        
        char* nextport;
        

        N_MAP::iterator itrN;

        //std::cout <<"-----Neighhbour Table-------" <<std::endl;
        
        for (itrN = ntable.begin(); itrN != ntable.end(); ++itrN) {

            //std::cout << "Through my neighbour: " << itrN->first << " I want to go to: " << nodeX <<std::endl;

            // check neighbour's DV entry & link cost
            DvX = (itrN->second.distancevectors).find(nodeX)->second;
            assert(DvX != -1); //Make sure Distance Vectors are not less than -1
            Cv = itrN->second.cost;
            assert(Cv != 0); //Checks that its cost is not 0 as this is it's distance to itself

            // remember port if we need to update our FT
            nextport = itrN->second.port;


            if(DvX != 0){

                newpathcost = Cv + DvX;

                if( currpathcost > newpathcost ) { // update table

                        currDV->find(nodeX)->second = newpathcost;
                        bfupdate = true;

                        // need to also update forward table!
                        ftable->find(nodeX)->second = nextport;
                        }

                    }
                //else std::cout << "bf: no update dvs";
             }
        }

    if(bfupdate) {
        std::cout << "bf(): updated\n";
        return 1;
    }
    else {
        std::cout << "bf(): no update\n";
        return 0;
    }

    //std::cout << "----------------------End of Bellman ford---------------------------------" << std::endl;
}
    
// returns 0 if no updates are made
//Name of this node , name of msg source, a newly created table, the neighbour table, our currentDV, our current FT, time stamps , neighbour history
bool dvupdate(char thisnode, char nodeX, DV_MAP newtable, N_MAP *ntable, DV_MAP *owntable, FT_MAP *ftable, TS_MAP *timeStamp, N_MAP history) {
    
    //std::cout << "\ndvupdate debug section\n";
    DV_MAP::iterator itrDV;
    for (itrDV = newtable.begin(); itrDV != newtable.end(); ++itrDV) {
        if((itrDV->second > 9999000)){
            return 0;
        }
    }
    DV_MAP oldtable = ntable->find(nodeX)->second.distancevectors;
    timeSeconds = time(NULL);
    //std::cout <<"Time at incoming" << (timeStamp->find(nodeX))->second << "\n" ;
    (timeStamp->find(nodeX))->second = timeSeconds;


    // std::cout <<"Time at Outgoing" << (timeStamp->find(nodeX))->second << "\n" ;
    char destnode;
    int  newcost;
    
    

    bool updateflag = false; // any changes to our neighbour DVs?
    
    
    if(deathAcked){
        //if(newtable.count(deadNode)==0){
            if(history.count(nodeX)==1 && ntable->count(nodeX)==0){
                std::cout<< "The neighbour node :"<< nodeX << "has been repaired \n";
                //newFileName << "The neighbour node :"<< nodeX << "has been repaired \n";
                // reinsert revived node into neighbour table and dvt
                ntable->insert(N_PAIR(nodeX,history.find(nodeX)->second));
                //assert(ntable->find(nodeX)!=NULL);
                owntable->insert(DV_PAIR(nodeX,history.find(nodeX)->second.cost));
                ftable->insert(FT_PAIR(nodeX,history.find(nodeX)->second.port));

                timeSeconds = time(NULL);
    
                (timeStamp->find(nodeX))->second = timeSeconds;

                timeStamp->insert(TS_PAIR(nodeX, timeSeconds));
                oldtable = ntable->find(nodeX)->second.distancevectors;

            }
            for (itrDV = newtable.begin(); itrDV != newtable.end(); ++itrDV) {
                
                //if(itrDV == newtable.begin()) std::cout << "start of table!";
                //if(itrDV == newtable.end()) std::cout << "end of table!";
                
                //std::cout << " end of table: " << newtable.end()->first << std::endl;
                // if old entry, delete it and insert updated entry
                
                destnode = itrDV->first;
                newcost = itrDV->second;
                //std::cout << "  checking entry..." << destnode << std::endl;

            // is this a node we haven't seen before? 

                if(destnode != thisnode) {
                    //std::cout << "  checking if node is not in owntable..." <<std::endl;

                    if((owntable->count(destnode) == 0)) {
                        std::cout << "Destnode" << destnode << " adding to table from  " << nodeX <<std::endl;
                        
                        owntable->insert(DV_PAIR(destnode, INT_MAX));
                    }

                    if(ftable->count(destnode) == 0) {
                        //std::cout << "      adding to forwarding table..." <<std::endl;
                        ftable->insert(FT_PAIR(destnode, "n/a"));
                    }
                }
            
            // do we need to update our knowledge of nodeX's DVs ?
                if(oldtable.count(destnode) != 0) {
                    
                    // found entry for node X

                    if(oldtable.find(destnode)->second == newcost) {
                        // duplicate entry, ignore
                    //  std::cout << itrDV->first;
                        //std::cout << " end of table: " << newtable.end()->first << std::endl;
                        //std::cout <<"       Continue" <<std::endl;
                        continue; 
                    }

                    //std::cout << "erased neighb DV for " << nodeX << 
                    //":" << oldtable.find(destnode)->first << " " << 
                    //oldtable.find(destnode)->second <<"\n";

                    // found node, but different entry? update the cost
                    (ntable->find(nodeX)->second.distancevectors).find(destnode)->second = newcost;
            
                    std::cout <<"dvtable(): updated NT for "<< nodeX << std::endl;
                }
                
                else { // if not found, insert new entry
                
                    std::cout <<destnode <<" adding new node to neighbour DV "<< nodeX <<std::endl;
                    (ntable->find(nodeX)->second.distancevectors).insert(DV_PAIR(destnode, newcost));

                    updateflag = true;
                
                //std::cout << "ntable: updated DV for " << nodeX << 
                //   ":" << destnode << " " << newcost <<"\n";
                }
            }
        //}  
    } 
    if(!updateflag) {
        //std::cout << "dvtable(): no update for " << nodeX << " DN = " << deadNode << std::endl;
        return 0;
    }
    //std::cout << "returning 1"<<std::endl;
    return 1;
}


std::string dvtostring(DV_MAP newtable, std::string nodename){

  DV_MAP::iterator itrDV;
  std::string message = "Type:CTRL\nSrc_Node:" + nodename + "\n"; //The message we will send;

  for(itrDV = newtable.begin();itrDV != newtable.end();++itrDV){

     char findest = itrDV->first;
    int newdist = itrDV->second;


    std::string newcost = std::to_string(newdist);

    message = message + findest+"," + newcost + "\n";  
  }

    message = message + "Z,";
    return message;

}

// example of function call...
// DV_MAP newdvupdate = stringtodv(msg, sourcenode);

DV_MAP stringtodv(std::string recvd_message, char* srcnode){

    // string will be a CTRL message as follows:

    // Src_Node:<srcnode>
    // <findest>,<totalcost>
    // <findest>,<totalcost>
    // ....
    // "\0"

    //*srcnode = 

    //std::string srcnode_str(srcnode);
    DV_MAP newtable;

    //-----------------------------------
    //           Source Node
    //---------------------------------- 

    int position_ctrl_A = recvd_message.find(":");
    recvd_message.erase(0,position_ctrl_A+1);
    int position_ctrl_B = recvd_message.find("\n");
    std::string src_router_name = recvd_message.substr(0,position_ctrl_B);
    recvd_message.erase(0, position_ctrl_B+1);

   
    
    *srcnode = src_router_name[0]; // char* = string ??? may need to c_str() or index [0]
    
    //---------------------------------- 

    //std::cout<<std::endl << "stringtodv table for " << src_router_name << std::endl;

    while(true){

        //--------------------------------------
        //     Final Destination Router Name
        //--------------------------------------
        int position_ctrl_1 = recvd_message.find(",");
        std::string destname = recvd_message.substr(0,position_ctrl_1);
        recvd_message.erase(0,position_ctrl_1+1);

        if(destname == "Z") {
        break;
        }

        //--------------------------------------
        //        Destination Router Cost
        //--------------------------------------

        int position_ctrl_2 = recvd_message.find("\n");
        std::string totalcost_str = recvd_message.substr(0,position_ctrl_2);
        recvd_message.erase(0, position_ctrl_2+1);
        int totalcost = std::stoi(totalcost_str); //Convert to int


        //------------------------------------------
        //   Insert <dest,cost> into DV table map
        //------------------------------------------

        newtable.insert(DV_PAIR(destname[0], totalcost));

        //std::cout<<std::endl << destname << "," << totalcost <<std::endl;
    }
    
  //  newFile << std::endl;
    
    return newtable;
}

//After removing a node we must reinitialise our DV table
// We set all values to nt max and then set the neighbour nodes values as accoridng to the neighbour table
void reinitDV(N_MAP *ntable, DV_MAP *currDV, D_MAP *dAcks){

    DV_MAP::iterator itrDV;
    N_MAP::iterator itrN;

    currDV->clear();
    for(itrN= ntable->begin();itrN != ntable->end();++itrN){
        dAcks->insert(D_PAIR(itrN->first,false)); 
        currDV->insert(DV_PAIR(itrN->first, itrN->second.cost)) ; 
        std::cout << "At Reinit the cost is " << itrN->second.cost << "To node " << itrN->first <<"\n" ;
        itrN->second.distancevectors.clear() ;
    }


}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}