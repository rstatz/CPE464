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

#define TCP_DEBUG_FLAG 1

#define MAX_CMD_LENGTH_BYTES 1400

#define TEXT_BLOCK_SIZE 200
#define MAX_NUM_TEXT_BLOCKS (MAX_CMD_LENGTH_BYTES/TEXT_BLOCK_SIZE)

#define FILL_BUFFER true
#define STOP_AT_SPACE false

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

// returns -1 on error
// otherwise return length of read plus the null terminator
int cli_get_next_string(char* cmd, int bufsize, char* buf, bool fillbuf) {
    char* cmd_start = cmd;
    int i = 0;
    
    while (isspace(*cmd) && !fillbuf) cmd++;

    if (*cmd == '\0')
        return -1;
    
    do {
        if (i == bufsize) {
            buf[bufsize - 1] = '\0';
            return bufsize;
        }
        if (isspace(cmd[i]) && !fillbuf)
            break;

        buf[i] = cmd[i];
        i++;
    } while(cmd[i] != '\0');

    buf[i] = '\0';
    return i + cmd - cmd_start;
}

// returns amount read
int cli_parse_msg_numh(char* cmd, uint8_t* numh) {
    char numh_s[20];
    int err, read = 0;

    // get number of dest handles
    read += (err = cli_get_next_string(cmd, 20, numh_s, STOP_AT_SPACE));
   
    if (err == -1) {
        printf("Invalid command format\n");
        return -1;
    }
    
    // check for digit
    if (isdigit(numh_s[0]))
        *numh = atoi(numh_s);
    else {
        *numh = 1;
        return 0;
    }

    if ((*numh <= 0) || (*numh > MAX_DEST_HANDLES)) {
        printf("Invalid number of destination handles, must be between 1 and 9\n");
        return -1;
    }

    return read;
}

int check_handle_length(char* handle) {
    int len = strlen(handle);
    
    if (len >= MAX_HANDLE_LENGTH_BYTES) {
       return -1;
    }

    return len;
}

// parse cmd for dest handles, returns number of characters read
int cli_parse_dest_handles(char* cmd, uint8_t* numh, char* handle_buf, char** hlist) {
    char trash[MAX_CMD_LENGTH_BYTES];
    char* cmd_start = cmd; 
    char* next_handle;
    int err, i;
    uint8_t numh_actual;

    for (i = 0, numh_actual = 0; i < *numh; i++, numh_actual++) {
        next_handle = handle_buf + (numh_actual * MAX_HANDLE_LENGTH_BYTES); 

        cmd += (err = cli_get_next_string(cmd, MAX_HANDLE_LENGTH_BYTES, next_handle, STOP_AT_SPACE));

        if (err == -1) {
            printf("Invalid command format\n");
            return -1;
        }
        if (err == (MAX_HANDLE_LENGTH_BYTES)) {
            printf("Invalid handle, handle longer than 100 characters: %s...\n", next_handle);   

            numh_actual--;
            cmd += 2; //reverses error
            cmd += cli_get_next_string(cmd, MAX_CMD_LENGTH_BYTES, trash, STOP_AT_SPACE); // clears out bad handle
        }
        else {
            DEBUG_PRINT("Dest handle %s\n", next_handle);

            hlist[numh_actual] = next_handle;
        }
    }

    *numh = numh_actual;

    return cmd - cmd_start;
}

uint8_t cli_parse_msg_text(char* cmd, char* text_buf, char** tlist) {
    char* cmdptr;
    char* text = text_buf;
    uint8_t numt = 0;

    do {
        text = text_buf + ((TEXT_BLOCK_SIZE) * numt);
        cmdptr = cmd + ((TEXT_BLOCK_SIZE - 1) * numt);

        tlist[numt++] = text;
    } while (cli_get_next_string(cmdptr, TEXT_BLOCK_SIZE, text, FILL_BUFFER) == TEXT_BLOCK_SIZE);

    return numt;
}

void cli_parse_msg(char* cmd, int sock, char* my_handle) {
    char handles[MAX_HANDLE_LENGTH_BYTES * MAX_DEST_HANDLES];
    char* hlist[MAX_DEST_HANDLES];
    char text_buf[MAX_CMD_LENGTH_BYTES];
    char* tlist[MAX_NUM_TEXT_BLOCKS];
    char* text;
    uint8_t numh, numt;
    int err, i;

    cmd += (err = cli_parse_msg_numh(cmd, &numh));
    
    if (err == -1)
        return;

    cmd += (err = cli_parse_dest_handles(cmd, &numh, handles, hlist));

    if (err == -1)
        return;
 
    numt = cli_parse_msg_text(cmd + 1, text_buf, tlist);

    // send message for every text block
    for (i = 0; i < numt; i++) {
        text = text_buf + (TEXT_BLOCK_SIZE * i);

        send_msg(sock, my_handle, numh, hlist, text);
    }
}

void cli_parse_broadcast(char* cmd, int sock, char* my_handle) {
    char text_buf[MAX_CMD_LENGTH_BYTES];
    char* tlist[MAX_NUM_TEXT_BLOCKS];
    char* text;
    int i, numt;

    numt = cli_parse_msg_text(cmd, text_buf, tlist);

    for (i = 0; i < numt; i++) {
        text = text_buf + (TEXT_BLOCK_SIZE * i);

        send_broadcast(sock, my_handle, text);
    }
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
    char server[100];

    sprintf(server, "%s.csc.calpoly.edu", unix_server);
    
    sock = tcpClientSetup(server, port);

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
