
// 	Writen - HMS April 2017
//  Supports TCP and UDP - both client and server


#ifndef __NETWORKS_H__
#define __NETWORKS_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BACKLOG 10

#define USE_TIMEOUT false

typedef struct UDPInfo {
    struct sockaddr_in6 addr;
    socklen_t addr_len;

    int sock;
} UDPInfo;

// sets up UDP socket
int get_UDP_socket();

//Safe sending and receiving 
int safeRecvfrom(int socketNum, void * buf, int len, UDPInfo*);
int safeSendtoErr(int socketNum, void * buf, int len, UDPInfo*);

// for the server side
int udpServerSetup(int portNumber);

// for the client side
int setupUdpClientToServer(UDPInfo*, char * hostName, int portNumber);

// returns true on timeout, else false
bool select_call(int sock, int32_t seconds, int microseconds, bool set_null);

#endif
