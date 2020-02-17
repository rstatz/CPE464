# udpCode makefile
# written by Hugh Smith - April 2017

CC = gcc
CFLAGS= -g -Wall

SRC = networks.c  networks.h gethostbyname.c gethostbyname.h
OBJS = networks.o gethostbyname.o 

all:  udpClient udpServer

udpClient: udpClient.c $(OBJS)
	$(CC) $(CFLAGS) -o udpClient udpClient.c $(OBJS)

udpServer$(FILE): udpServer.c $(OBJS)
	$(CC) $(CFLAGS) -o udpServer udpServer.c  $(OBJS)

	
.c.o: $SRC
	gcc -c $(CFLAGS) $< -o $@ $(LIBS)

cleano:
	rm -f *.o

clean:
	rm -f udpServer udpClient *.o

