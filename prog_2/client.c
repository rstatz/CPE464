#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/select.h>

#include "debug.h"
#include "networks.h"
#include "client_table.h"
#include "chat_packets.h"

#define TCP_DEBUG_FLAG 1

#define PROMPT "$:"

void check_args(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: cclient handle_name server port_number\n");
        exit(EXIT_FAILURE);
    }
}

void check_stdin() {
    
}

bool check_server_socket(int sock) {
    bool run = true;
    int flag, pdulen;

    flag = read_chat_header(sock, &pdulen);

    switch(flag) {
        case(FLAG_HANDLE_ACK) :
            break;
        default:
            printf("Error: Unrecognized packet\n");
    }

    return run;
}

void start_chat_client(int sock) {
    fd_set rfds;
    bool run = true;
    int stdin_fd = fileno(stdin);
    int max_fd = (stdin_fd > sock) ? stdin_fd + 1 : sock + 1;

    FD_ZERO(&rfds);
    FD_SET(stdin_fd, &rfds);
    FD_SET(sock, &rfds);

    printf(PROMPT);

    while(run) {
        select(max_fd, &rfds, NULL, NULL, NULL);
    
        if (FD_ISSET(stdin_fd, &rfds)) {
            check_stdin();
            printf(PROMPT);
        }

        if (FD_ISSET(sock, &rfds))
            run = check_server_socket(sock);
    }
}

int connect_server(char* unix_server, char* port) {
    int sock;
    char server[100];

    sprintf(server, "%s.csc.calpoly.edu", unix_server);
    
    sock = tcpClientSetup(server, port);

    return sock;
}

int main(int argc, char* argv[]) {
    int sock;

    check_args(argc, argv);

    sock = connect_server(argv[2], argv[3]);

    send_handle_req(sock, argv[1]);

//    start_chat_client(sock);
    close(sock);

    exit(EXIT_SUCCESS);
}
