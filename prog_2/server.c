#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/select.h>

#include "networks.h"
//#include "chat_test.h"
//#include "chat_parse.h"
#include "client_table.h"
#include "chat_packets.h"

#define DEBUG_FLAG 1

#define CHAT_SOCKET_CLOSE 0

void usage() {
    fprintf(stderr, "Usage: server [port_num]\n");
    exit(EXIT_FAILURE);
}

int check_args(int argc, char* argv[]) {
    if (argc > 2)
        usage();
    
    if (argc < 2)
        return 0; // user did not specify port

    return atoi(argv[1]); //user specified port
}

int check_server_socket(int server_sock, int* active_fds, int* max_fd, fd_set* rfds) {
    int new_sock = 0;

    if (FD_ISSET(server_sock, rfds)) {
        // add new client
        new_sock = tcpAccept(server_sock, DEBUG_FLAG);
        FD_SET(new_sock, rfds);

        *max_fd = (new_sock >= *max_fd) ? new_sock + 1 : *max_fd;
        (*active_fds)--;
    }

    return new_sock;
}

// gets mac socket number in client table
int get_max_fd(Client_Table* ctable) {
    Client_Info* ci;
    void* stream;
    int max_fd = 0;

    stream = ct_get_stream(ctable);

    while((ci = ct_get_next_entry(stream)) != NULL)
        max_fd = (ci->sock >= max_fd) ? ci->sock + 1 : max_fd;

    return max_fd;
}

void chat_handle_req(Client_Table* ctable, int sock) {
    // recv handle
    // if invalid or in use
    //      send err
    // else
    //      send ack
}

void chat_broadcast(Client_Table* ctable, int sock) {

}

void read_client_socket(Client_Table* ctable, int sock, int* max_fd, fd_set* rfds) {
    int flag, pdu_length;

    flag = read_chat_header(sock, &pdu_length);

    switch(flag) {
        case(CHAT_SOCKET_CLOSE) :
            FD_CLR(sock, rfds);
            *max_fd = get_max_fd(ctable) + 1;
            break;
        case(FLAG_HANDLE_REQ) :
            chat_handle_req(ctable, sock);
            break;
        case(FLAG_BROADCAST) :
            chat_broadcast(ctable, sock);
            break;
        case(FLAG_MSG) :
            break;
        case(FLAG_EXIT) :
            break;
        case(FLAG_HLIST_REQ) :
            break;
        default:
            // TODO: figure out what to do
            fprintf(stderr, "Server: Unrecognized Packet");
            //trash_packet(sock, pdu_length - sizeof(Chat_header));
            break;
    }
}

void check_client_sockets(Client_Table* ctable, int active_fds, int* max_fd, fd_set* rfds) {
    void* stream;
    Client_Info* entry;

    stream = ct_get_stream(ctable);

    while (active_fds > 0) {
        entry = ct_get_next_entry(stream);
        
        if (FD_ISSET(entry->sock, rfds)) {
            read_client_socket(ctable, entry->sock, max_fd, rfds);

            active_fds--;
        }
    }
}

void start_chat_server(int server_sock) {
    fd_set rfds;
    int active_fds, new_sock;
    int max_fd = server_sock + 1;
    Client_Table* ctable;

    ctable = new_ctable();
    
    FD_ZERO(&rfds);
    FD_SET(server_sock, &rfds);

    while(1) {
        active_fds = select(max_fd, &rfds, NULL, NULL, NULL);

        new_sock = check_server_socket(server_sock, &active_fds, &max_fd, &rfds); 
        
        // add a new client
        if (new_sock > 0)
            new_client(ctable, new_sock);
        
        check_client_sockets(ctable, active_fds, &max_fd, &rfds);

        max_fd = (server_sock >= max_fd) ? server_sock + 1 : max_fd;
    }
}

int main(int argc, char* argv[]) {
    int port, server_sock;

    port = check_args(argc, argv);

    server_sock = tcpServerSetup(port); // open server with port number

    start_chat_server(server_sock); // monitor socket

    exit(EXIT_FAILURE);
}
