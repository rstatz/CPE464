
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

typedef struct udp_info {
    // udp stuff to pass in below
} upd_info;

//Safe sending and receiving 
int safeRecvfrom(int socketNum, void * buf, int len, int flags, struct sockaddr *srcAddr, int * addrLen);
int safeSendto(int socketNum, void * buf, int len, int flags, struct sockaddr *srcAddr, int addrLen);

// for the server side
int udpServerSetup(int portNumber);

// for the client side
int setupUdpClientToServer(struct sockaddr_in6 *server, char * hostName, int portNumber);


#endif
