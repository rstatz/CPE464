#ifndef CHAT_PACKETS_H
#define CHAT_PACKETS_H

#include <stdint.h>

typedef struct Chat_header {
    uint16_t pdu_length;
    uint8_t flag;
} __attribute__((packed)) Chat_header;

typedef struct Chat_handle {
    uint8_t handle_length;
    
    char handle;
} __attribute__((packed)) Chat_handle;

// Flag = 1 : Client connection
// |header|source handle|
#define FLAG_HANDLE_REQ 1
void send_handle_req(int sock, char* handle);

// Flag = 2 : Server ACK for Client Connection
// |header|
#define FLAG_HANDLE_ACK 2
void send_handle_ack(int sock);

// Flag = 3 : Server ERR for Client Connection
// |header|
#define FLAG_HANDLE_INUSE 3
void send_handle_inuse(int sock);

// Flag = 4 : Broadcast
// |header|source handle|text|
#define FLAG_BROADCAST 4
void send_broadcast(int sock, char* handle, char* text);

// Flag = 5 : Message
// |header|source handle|dest handles...|text|
#define FLAG_MSG 5
void send_msg(int sock, int, char** handles, char* text);

// Flag = 7 : Handle name error
// |header|incorrect dest handle|
#define FLAG_HANDLE_ERR 7
void send_handle_err(int sock, char* handle);

// Flag = 8 : Client Exit Request
// |header|
#define FLAG_EXIT_REQ 8
void send_exit_req(int sock);

// Flag = 9 : Client Exit ACK
// |header|
#define FLAG_EXIT_ACK 9
void send_exit_ack(int sock);

// Flag = 10 : Handle List Request
// |header|
#define FLAG_HLIST_REQ 10 
void send_hlist_req(int sock);

// Flag = 11 : Number of Clients
// |header|number|
#define FLAG_HLIST_NUM 11
void send_hlist_num(int sock, uint32_t hnum);

// Flag = 12 : Client Handle Entry
// |header|handle|
#define FLAG_HLIST_ENTRY 12
void send_hlist_entry(int sock, char* handle);

// Flag = 13 : Handle List End
// |header|
#define FLAG_HLIST_END 13
void send_hlist_end(int sock);

void read_chat_handle(int socket, char* hbuf);
int read_chat_header(int socket, int* pdu_length);
void trash_packet(int socket, int length);

#endif
