#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>

#include "debug.h"
#include "networks.h"
//#include "chat_test.h"
//#include "chat_parse.h"
#include "client_table.h"
#include "chat_packets.h"

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

// gets mac socket number in client table
int get_max_fd(Client_Table* ctable) {
    Client_Info* ci;
    void* stream;
    int max_fd = 0;

    stream = ct_get_stream(ctable);

    while((ci = ct_get_next_entry(&stream)) != NULL)
        max_fd = (ci->sock >= max_fd) ? ci->sock + 1 : max_fd;

    return max_fd;
}

void chat_close_client(Client_Table* ctable, int sock, int* max_fd, fd_set* rfds) {
    FD_CLR(sock, rfds);
    close(sock);

    rm_client(ctable, sock);
    
    *max_fd = get_max_fd(ctable) + 1;
}

void chat_handle_req(Client_Table* ctable, int sock) {
    char handle[MAX_HANDLE_LENGTH_BYTES];
    
    read_chat_handle(sock, handle);
    
    // Checks if handle is taken already
    if (ct_get_socket(ctable, handle) != -1) {
        send_handle_inuse(sock);
        DEBUG_PRINT("Handle Taken: %s\n", handle);
    }
    else {
        ct_set_handle(ctable, sock, handle);
        send_handle_ack(sock);
        DEBUG_PRINT("Handle Connected: %s\n", handle);
    } 
}

void chat_broadcast(Client_Table* ctable, int sock) {
    Client_Info* ci;
    char handle[MAX_HANDLE_LENGTH_BYTES];
    char text[MAX_TEXT_LENGTH_BYTES];
    void* stream = ct_get_stream(ctable);

    read_broadcast(sock, handle, text);

    ci = ct_get_next_entry(&stream);

    while(ci != NULL) {
        if (ci->sock != sock)
            send_broadcast(ci->sock, handle, text);
        ci = ct_get_next_entry(&stream);
    }
}

// returns number of destination handles
int server_parse_msg(int sock, char* src_handle, char* dest_handles, char* text) {
    char* ptr;
    int i;
    uint8_t numh;

    // read packet
    read_chat_handle(sock, src_handle);
    numh = read_chat_length(sock);

    for (i = 0; i < numh; i++) {
        ptr = dest_handles + (MAX_HANDLE_LENGTH_BYTES * i);
        read_chat_handle(sock, ptr);
    }
    
    read_chat_text(sock, text);

    return numh;
}

void chat_msg(Client_Table* ctable, int src_sock) {
    char src_handle[MAX_HANDLE_LENGTH_BYTES];
    char text[MAX_TEXT_LENGTH_BYTES];
    char dest_handles[MAX_HANDLE_LENGTH_BYTES * MAX_DEST_HANDLES];
    char* hlist[1];
    int i, numh, dest_sock;

    numh = server_parse_msg(src_sock, src_handle, dest_handles, text);

    // send packets
    for (i = 0; i < numh; i++) {
        hlist[0] = &dest_handles[MAX_HANDLE_LENGTH_BYTES * i];

        // check if client exists
        if ((dest_sock = ct_get_socket(ctable, hlist[0])) == -1) {
            DEBUG_PRINT("%s attempted to send message to %s\n", src_handle, hlist[0]);
            send_dest_handle_err(src_sock, hlist[0]);
        }
        else {
            DEBUG_PRINT("%s sending message to %s\n", src_handle, hlist[0]); 
            send_msg(dest_sock, src_handle, 1, hlist, text);
       }
    }
}

void chat_exit_req(Client_Table* ctable, int sock, int* max_fd, fd_set* rfds) {
    send_exit_ack(sock);
    chat_close_client(ctable, sock, max_fd, rfds);
}

void chat_send_hlist(Client_Table* ctable, int sock) {
    Client_Info* ci;
    void* stream = ct_get_stream(ctable);
    int numh = get_num_clients(ctable);
    
    send_hlist_num(sock, numh);

    while ((ci = ct_get_next_entry(&stream)) != NULL) {
        send_hlist_entry(sock, ci->handle);
    }

    send_hlist_end(sock);
}

void read_client_socket(Client_Table* ctable, int sock, int* max_fd, fd_set* rfds) {
    int flag, pdu_length;

    flag = read_chat_header(sock, &pdu_length);

    switch(flag) {
        case(CHAT_SOCKET_CLOSED) :
            chat_close_client(ctable, sock, max_fd, rfds);
            DEBUG_PRINT("%s terminated\n", ct_get_handle(ctable, sock));
            break;
        case(FLAG_HANDLE_REQ) :
            chat_handle_req(ctable, sock);
            break;
        case(FLAG_BROADCAST) :
            DEBUG_PRINT("Routing broadcast\n"); 
            chat_broadcast(ctable, sock);
           break;
        case(FLAG_MSG) :
            DEBUG_PRINT("Routing Message\n");
            chat_msg(ctable, sock);
            break;
        case(FLAG_EXIT_REQ) :
            chat_exit_req(ctable, sock, max_fd, rfds);
            break;
        case(FLAG_HLIST_REQ) :
            chat_send_hlist(ctable, sock);
            break;
        default:
            fprintf(stderr, "Server: Unrecognized Packet\n");
            trash_packet(sock, pdu_length - sizeof(Chat_header));
            break;
    }
}

void check_client_sockets(Client_Table* ctable, int active_fds, int* max_fd, fd_set* rfds) {
    void* stream;
    Client_Info* entry;

    stream = ct_get_stream(ctable);

    while (active_fds > 0) {
        entry = ct_get_next_entry(&stream);
        
        if (FD_ISSET(entry->sock, rfds)) {
            read_client_socket(ctable, entry->sock, max_fd, rfds);

            active_fds--;
        }
    }
}

void check_server_socket(Client_Table* ctable, int server_sock, int* active_fds, int* max_fd, fd_set* rfds) {
    int new_sock;

    if (FD_ISSET(server_sock, rfds)) {
        // add new client
        new_sock = tcpAccept(server_sock);

        DEBUG_PRINT("Accepted new client on socket %d\n", new_sock);

        new_client(ctable, new_sock);

        *max_fd = (new_sock >= *max_fd) ? new_sock + 1 : *max_fd;
        (*active_fds)--;
    }
}

void build_fd_set(fd_set* rfds, Client_Table* ctable, int server_sock) {
    void* stream;
    Client_Info* ci;

    stream = ct_get_stream(ctable);

    FD_SET(server_sock, rfds);

    while ((ci = ct_get_next_entry(&stream)) != NULL) {
        FD_SET(ci->sock, rfds);
    }
}

void start_chat_server(int server_sock) {
    fd_set rfds;
    int active_fds;
    int max_fd = server_sock + 1;
    Client_Table* ctable;

    ctable = new_ctable();
    
    FD_ZERO(&rfds);
    
    while(1) {
        DEBUG_PRINT("SELECT\n");
        build_fd_set(&rfds, ctable, server_sock);
        active_fds = select(max_fd, &rfds, NULL, NULL, NULL);
        DEBUG_PRINT("WOKE\n");

        check_server_socket(ctable, server_sock, &active_fds, &max_fd, &rfds); 
               
        check_client_sockets(ctable, active_fds, &max_fd, &rfds);

        max_fd = (server_sock >= max_fd) ? server_sock + 1 : max_fd;
        DEBUG_PRINT("max_fd = %d\n", max_fd);
    }
}

int main(int argc, char* argv[]) {
    int port, server_sock;

    port = check_args(argc, argv);

    server_sock = tcpServerSetup(port); // open server with port number

    start_chat_server(server_sock); // monitor socket

    exit(EXIT_FAILURE);
}
