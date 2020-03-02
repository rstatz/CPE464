
// 	Writen - HMS April 2017
//  Supports TCP and UDP - both client and server


#ifndef __NETWORKS_H__
#define __NETWORKS_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BACKLOG 10

typedef struct UDPInfo {
    int sock;
    float err;

    struct sockaddr_in6 addr;
    uint32_t addr_len;
} UDPInfo;

// sets up UDP socket
int get_UDP_socket();

//Safe sending and receiving 
int safeRecvfrom(int socketNum, void * buf, int len, int flags, struct sockaddr_in6 *srcAddr, uint32_t * addrLen);
int safeSendto(int socketNum, void * buf, int len, int flags, struct sockaddr_in6 *srcAddr, uint32_t addrLen);

// for the server side
int udpServerSetup(int portNumber);

// for the client side
int setupUdpClientToServer(struct sockaddr_in6 *server, char * hostName, int portNumber);


#endif
