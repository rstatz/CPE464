# Makefile for CPE464 tcp test code
# written by Hugh Smith - April 2019

CC= gcc
CFLAGS= -g -Wall -Werror
LIBS = 


all:   cclient server

cclient: client.c chat_client_cli.o chat_packets.o networks.o gethostbyname6.o
	$(CC) $(CFLAGS) -o cclient client.c chat_client_cli.o chat_packets.o networks.o gethostbyname6.o $(LIBS)

server: server.c client_table.o chat_packets.o networks.o linkedlist.o gethostbyname6.o
	$(CC) $(CFLAGS) -o server server.c client_table.o chat_packets.o linkedlist.o networks.o gethostbyname6.o $(LIBS)

.c.o:
	gcc -c $(CFLAGS) $< -o $@ $(LIBS)

cleano:
	rm -f *.o

clean:
	rm -f server cclient *.o




