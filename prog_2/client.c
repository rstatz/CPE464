#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/select.h>

#include "debug.h"
#include "networks.h"
#include "client_table.h"
#include "chat_packets.h"
#include "chat_client_cli.h"

#define TCP_DEBUG_FLAG 1

#define PROMPT "$: "

void check_args(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: cclient handle_name server port_number\n");
        exit(EXIT_FAILURE);
    }
}

void check_client_handle(char* handle) {
    if (isdigit(handle[0])) {
        printf("Invalid handle, handle starts with a number\n");
        exit(EXIT_FAILURE);
    }
    if (strlen(handle) > MAX_HANDLE_LENGTH_BYTES - 1) {
        printf("Invalid handle, handle, longer than 100 characters: %s\n", handle);
        exit(EXIT_FAILURE);
    }
}

void print_err(char* err, char* str) {
    printf(err, str);
    printf(PROMPT);
}

void print_msg(char* handle, char* text) {
    printf("\n%s:%s\n", handle, text);
    printf(PROMPT);
}

void flush_stdin() {
    while((getchar()) != '\n');
}

int check_handle_length(char* handle) {
    int len = strlen(handle);
    
    if (len >= MAX_HANDLE_LENGTH_BYTES) {
       return -1;
    }

    return len;
}

// returns -1 if command exceeds max length
int check_cmd_length(char* cmd) {
    if (cmd[MAX_CMD_LENGTH_BYTES] != 0) {
        printf("Input exceeded %d characters\n", MAX_CMD_LENGTH_BYTES);
        flush_stdin();
        return -1;
    }
    
    return 1;
}

void client_read_broadcast(int sock) {
    char handle[MAX_HANDLE_LENGTH_BYTES];
    char text[MAX_TEXT_LENGTH_BYTES];

    read_broadcast(sock, handle, text);

    print_msg(handle, text);
}

void client_read_msg(int sock) {
    char src_handle[MAX_HANDLE_LENGTH_BYTES];
    char dest_handle[MAX_HANDLE_LENGTH_BYTES];
    char text[MAX_TEXT_LENGTH_BYTES];

    read_chat_handle(sock, src_handle);
    read_chat_length(sock);
    read_chat_handle(sock, dest_handle);
    read_chat_text(sock, text);

    print_msg(src_handle, text);
}

void client_read_handle_err(int sock) {
    char handle[MAX_HANDLE_LENGTH_BYTES];
    
    read_chat_handle(sock, handle);

    print_err("\nClient with handle %s does not exist\n", handle);
}

void client_read_hlist_num(int sock) {
    int numh;

    numh = read_hlist_num(sock);

    printf("Number of clients: %d\n", numh);
}

void client_read_hlist_entry(int sock) {
    char handle[MAX_HANDLE_LENGTH_BYTES];
    
    read_chat_handle(sock, handle);

    printf("    %s\n", handle);
}

void check_stdin(int sock, char* my_handle, bool* stdin_en) {
    char cmd[MAX_CMD_LENGTH_BYTES + 2];
    
    memset((void*)cmd, 0, MAX_CMD_LENGTH_BYTES + 2);

    fgets(cmd, MAX_CMD_LENGTH_BYTES + 2, stdin);

    if (check_cmd_length(cmd) == -1)
        return;

    if (cmd[0] != '%')
        printf("Invalid command\n");
    else if (!isspace(cmd[2]))
        printf("Invalid command\n");
    else {
        switch(tolower(cmd[1])) {
            case('m') :
                cli_parse_msg(cmd + 3, sock, my_handle);
                break;
            case('b') :
                cli_parse_broadcast(cmd + 3, sock, my_handle);
                break;
            case('l') :
                send_hlist_req(sock);
                *stdin_en = false;
                break;
            case('e') :
                send_exit_req(sock);
                *stdin_en = false;
                break;
            default :
                printf("Invalid command\n");
        }
    }
}

bool check_server_socket(int sock, char* my_handle, bool* stdin_en) {
    bool run = true;
    int flag, pdulen;

    flag = read_chat_header(sock, &pdulen);

    switch(flag) {
        case(CHAT_SOCKET_CLOSED) :
            run = false;
            printf("\nServer Terminated\n");
            break;
        case(FLAG_HANDLE_ACK) :
            *stdin_en = true;
            printf(PROMPT);
            DEBUG_PRINT("Handle Accepted by Server\n");
            break;
        case(FLAG_HANDLE_INUSE) :
            run = false;
            printf("Handle already in use: %s\n", my_handle);
            break;
        case(FLAG_BROADCAST) :
            client_read_broadcast(sock);
            break;
        case(FLAG_MSG) :
            client_read_msg(sock);
            break;
        case(FLAG_HANDLE_ERR) :
            client_read_handle_err(sock);
            break;
        case(FLAG_EXIT_ACK) :
            run = false;
            break;
        case(FLAG_HLIST_NUM) :
            client_read_hlist_num(sock);
            break;
        case(FLAG_HLIST_ENTRY) :
            client_read_hlist_entry(sock);
            break;
        case(FLAG_HLIST_END) :
            *stdin_en = true;
            printf(PROMPT);
            DEBUG_PRINT("HList Finished\n");
            break;
        default:
            printf("Error: Unrecognized packet\n");
    }

    return run;
}

void start_chat_client(int sock, char* my_handle) {
    fd_set rfds;
    bool run = true, stdin_en = false;
    int stdin_fd = fileno(stdin);
    int max_fd = (stdin_fd > sock) ? stdin_fd + 1 : sock + 1;

    send_handle_req(sock, my_handle); 
    
    FD_ZERO(&rfds);

    while(run) {
        if (stdin_en)
            FD_SET(stdin_fd, &rfds);
        FD_SET(sock, &rfds);

        DEBUG_PRINT("SELECT\n");
        select(max_fd, &rfds, NULL, NULL, NULL);
        DEBUG_PRINT("WOKE\n");

        if (FD_ISSET(stdin_fd, &rfds)) {
            check_stdin(sock, my_handle, &stdin_en);
            
            if (stdin_en)
                printf(PROMPT);
        }

        if (FD_ISSET(sock, &rfds)) {
            DEBUG_PRINT("HANDLING SERVER\n");
            run = check_server_socket(sock, my_handle, &stdin_en);
        }
    }

    close(sock);
}

int connect_server(char* unix_server, char* port) {
    int sock;
    //char server[100];

    //sprintf(server, "%s.csc.calpoly.edu", unix_server);
    
    sock = tcpClientSetup(unix_server, port);

    return sock;
}

int main(int argc, char* argv[]) {
    int sock;

    setvbuf(stdout, NULL, _IONBF, 0); // turn off printf buffering

    check_args(argc, argv);
    check_client_handle(argv[1]);

    sock = connect_server(argv[2], argv[3]);

    start_chat_client(sock, argv[1]);

    exit(EXIT_SUCCESS);
}
