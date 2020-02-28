# udpCode makefile
# written by Hugh Smith - April 2017

CC = gcc
CFLAGS= -g -Wall

SRC = rcopy_packets.c window.c networks.c gethostbyname.c
SERVER_SRC = server.c
RCOPY_SRC = rcopy.c
OBJS = $(SRC:.c=.o) 

all:  rcopy server

rcopy: rcopy.c $(OBJS)
	$(CC) $(CFLAGS) -o client $(RCOPY_SRC) $(OBJS)

server: server.c $(OBJS)
	$(CC) $(CFLAGS) -o server $(SERVER_SRC)  $(OBJS)

	
.c.o: $SRC
	gcc -c $(CFLAGS) $< -o $@ $(LIBS)

cleano:
	rm -f *.o

clean:
	rm -f udpServer udpClient *.o

