# Makefile for CPE464 tcp test code
# written by Hugh Smith - April 2019

CC= gcc
CFLAGS= -g -Wall
LIBS = 


all:   client server

client: client.c networks.o gethostbyname6.o
	$(CC) $(CFLAGS) -o client client.c networks.o gethostbyname6.o $(LIBS)

server: server.c networks.o gethostbyname6.o
	$(CC) $(CFLAGS) -o server server.c client_table.c chat_packets.c linkedlist.c networks.o gethostbyname6.o $(LIBS)

.c.o:
	gcc -c $(CFLAGS) $< -o $@ $(LIBS)

cleano:
	rm -f *.o

clean:
	rm -f server client *.o




