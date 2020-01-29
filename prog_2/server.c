#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/select.h>

#include "networks.h"
//#include "chat_test.h"
//#include "chat_parse.h"
#include "client_table.h"

void usage() {
    fprintf(STDERR, "Usage: server [port_num]\n");
    exit(EXIT_FAILURE);
}

int check_args(int argc, char* argv[]) {
    if (argc > 2)
        usage();
    
    if (argc < 2)
        return 0; // user did not specify port

    return atoi(argv[1]); //user specified port
}

void read_clients(Client_Table* ctable, int active_fds, int* nfds, fd_set* rfds) {
    void* stream;
    Client_Info* entry;

    stream = ct_get_stream(ctable);

    while (active_fds > 0) {
        entry = ct_get_next_entry(stream);
        
        if (FD_ISSET(entry->socket)) {
            // read and parse that shit
            // manage nfds

            active_fds--;
        }
    }
}

void start_chat_server(int server_sock) {
    fd_set rfds;
    int active_fds;
    int nfds = server_sock + 1;
    Client_Table* ctable;

    ctable = new_ctable();
    
    FD_ZERO(&rfds);
    FD_SET(server_sock, &rfds);

    while(1) {
        active_fds = select(nfds, rfds, NULL, NULL, NULL);

        // check server socket
        if (FD_ISSET(server_sock)) {
            // add new client
            // manage nfds
            active_fds--;
        }
        
        // check client sockets
        read_clients(ctable, active_fds, &nfds, &rfds);
    }
}

int main(int argc, char* argv[]) {
    int port, server_sock;
    Client_Table* table;

    port = check_args(argc, argv);

    server_sock = tcpServerSetup(port); // open server with port number

    start_chat_server(server_sock); // monitor socket

    exit(EXIT_FAILURE);
}
