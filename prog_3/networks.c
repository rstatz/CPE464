
// Hugh Smith April 2017
// Network code to support TCP/UDP client and server connections

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "cpe464.h"
#include "debug.h"
#include "networks.h"
#include "gethostbyname.h"

int get_UDP_socket() {
    int sock;
    
    if ((sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
        perror("get_UDP_socket: ");
        exit(EXIT_FAILURE);
    }
    
    return sock;
}

int safeRecvfrom(int socketNum, void* buf, int len, UDPInfo* udp)
{
	int returnValue = 0;
	if ((returnValue = recvfrom(socketNum, buf, (size_t) len, 0, (struct sockaddr*)&udp->addr, &udp->addr_len)) < 0)
	{
		perror("recvfrom: ");
		exit(-1);
	}
	
	return returnValue;
}

int safeSendtoErr(int socketNum, void* buf, int len, UDPInfo* udp)
{
	int returnValue = 0;
	if ((returnValue = sendtoErr(socketNum, (void*)buf, (size_t)len, 0, (struct sockaddr*)&udp->addr, (socklen_t)udp->addr_len)) < 0)
	{
		perror("sendto");
		exit(-1);
	}
	
	return returnValue;
}

int udpServerSetup(int portNumber)
{
	struct sockaddr_in6 server = {0};
	int socketNum = 0;
	int serverAddrLen = 0;	
	
	// create the socket
	socketNum = get_UDP_socket();
	
	// set up the socket
	server.sin6_family = AF_INET6;    		// internet (IPv6 or IPv4) family
	server.sin6_addr = in6addr_any ;  		// use any local IP address
	server.sin6_port = htons(portNumber);   // if 0 = os picks 

	// bind the name (address) to a port
	if (bind(socketNum,(struct sockaddr *) &server, sizeof(server)) < 0)
	{
		perror("bind() call error");
		exit(-1);
	}

	/* Get the port number */
	serverAddrLen = sizeof(server);
	getsockname(socketNum,(struct sockaddr *) &server,  (socklen_t *) &serverAddrLen);
	printf("Server using Port #: %d\n", ntohs(server.sin6_port));

	return socketNum;	
	
}

int setupUdpClientToServer(UDPInfo* udp, char * hostName, int portNumber)
{
	// currently only setup for IPv4 
	int socketNum = 0;
	char ipString[INET6_ADDRSTRLEN];
	uint8_t * ipAddress = NULL;
	
    memset((void*)&udp->addr, 0, sizeof(struct sockaddr_in6));
    udp->addr_len = sizeof(struct sockaddr_in6);

	// create the socket
	socketNum = get_UDP_socket();
  	 	
	if ((ipAddress = gethostbyname6(hostName, &udp->addr)) == NULL)
	{
		exit(-1);
	}
	
	udp->addr.sin6_port = ntohs(portNumber);
	udp->addr.sin6_family = AF_INET6;	
	
	inet_ntop(AF_INET6, ipAddress, ipString, sizeof(ipString));
	printf("Server info - IP: %s Port: %d \n", ipString, portNumber);
		
	return socketNum;
}

bool select_call(int sock, int seconds, int microseconds, bool set_null) {
    fd_set rfds;
    struct timeval aTimeout;
    struct timeval *timeout = NULL;

    if (!set_null) {
        aTimeout.tv_sec = seconds;
        aTimeout.tv_usec = microseconds;
        timeout = &aTimeout;
    }

    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);    

    //DEBUG_PRINT("SELECT\n");
    if(select(sock + 1, &rfds, (fd_set*)0, (fd_set*)0, timeout) < 0) {
        perror("select");
        exit(EXIT_FAILURE);
    }
    //DEBUG_PRINT("WOKE\n");
 
    if (FD_ISSET(sock, &rfds))
        return false;
    return true;
}
