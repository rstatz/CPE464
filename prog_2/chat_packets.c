#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "debug.h"
#include "chat_packets.h"

#define RECV_FLAGS MSG_WAITALL
#define SEND_FLAGS 0

#define HANDLE_LENGTH_BYTES 1

#define MAX_HEAD_HANDLE_LENGTH_BYTES (101 + sizeof(Chat_header))
#define MAX_HEAD_NUM_PACKET_LENGTH_BYTES (4 + sizeof(Chat_header))
#define MAX_MSG_PACKET_LENGTH_BYTES (1211 + sizeof(Chat_header))
#define MAX_BROADCAST_PACKET_LENGTH_BYTES (301 + sizeof(Chat_header))

static int chat_recv(int socket, void* buf, int length, int flags) {
    int out;
    
    DEBUG_PRINT("READING %d\n", length);
    if((out = recv(socket, buf, length, flags)) == -1)
        perror("recv call");

    return out;
}

static int chat_send(int socket, void* buf, int length, int flags) {
    int out;

    DEBUG_PRINT("SENDING %d\n", length);
    if ((out = send(socket, buf, length, flags)) == -1)
        perror("send call");

    return out;
}

int read_chat_header(int socket, int* pdu_length) {
    int bytes;
    Chat_header head;

    bytes = chat_recv(socket, (void*)&head, sizeof(Chat_header), RECV_FLAGS);

    // socket has closed
    if (bytes == 0)
        return 0;

    *pdu_length = ntohs(head.pdu_length);

    return head.flag;
}

// hbuf must be of at least size 101 bytes
void read_chat_handle(int socket, char* hbuf) {
    uint8_t length;

    chat_recv(socket, (void*)&length, HANDLE_LENGTH_BYTES, RECV_FLAGS);
    chat_recv(socket, (void*)hbuf, length, RECV_FLAGS);

    hbuf[length] = '\0';
}

void read_chat_text(int sock, char* tbuf) {
    char* ptr = tbuf;

    chat_recv(sock, (void*)ptr, sizeof(char), RECV_FLAGS);
 
    // Read char until max text length or null character reached
    while ((*ptr != '\0') && ((ptr - tbuf) < MAX_TEXT_LENGTH_BYTES)) {
       ptr++;
       chat_recv(sock, (void*)ptr, sizeof(char), RECV_FLAGS);
    }
}

uint8_t read_chat_length(int sock) {
    uint8_t len;

    chat_recv(sock, (void*)&len, 1, RECV_FLAGS);

    return len;
}

void trash_packet(int socket, int length) {
    char buf[2000];

    chat_recv(socket, (void*)buf, length, RECV_FLAGS);
}

// returns amount written in bytes
static int write_header(void* pack, int pdu_length, int flag) {
    Chat_header head;
    
    head.pdu_length = htons(pdu_length);
    head.flag = flag;
    
    memcpy(pack, (void*)&head, sizeof(Chat_header));

    return sizeof(Chat_header);
}

static int write_handle(void* pack, char* handle) {
    uint8_t hlen = strlen(handle);

    memcpy(pack, (void*)&hlen, HANDLE_LENGTH_BYTES);
    pack = (void*)(((char*)pack) + HANDLE_LENGTH_BYTES);
    memcpy(pack, (void*)handle, hlen);

    return hlen + HANDLE_LENGTH_BYTES;
}

static int write_text(void* pack, char* text) {
    int tlen = strlen(text);
    
    memcpy(pack, (void*)text, tlen);
    
    if ((tlen > 0) && (text[tlen - 1] == '\n')) {
        ((char*)pack)[tlen - 1] = '\0';
        return tlen;
    }
    
    ((char*)pack)[tlen] = '\0';

    return tlen + 1;
}

static void send_header_packet(int sock, int flag) {
    char* pack[sizeof(Chat_header)];
    
    write_header((void*)pack, sizeof(Chat_header), flag);

    chat_send(sock, (void*)&pack[0], sizeof(Chat_header), SEND_FLAGS);
}

static void send_header_handle_packet(int sock, char* handle, int flag) {
    int pdulen;
    char pack[MAX_HEAD_HANDLE_LENGTH_BYTES];
    char* ptr = &pack[0];
    
    pdulen = sizeof(Chat_header) + HANDLE_LENGTH_BYTES + strlen(handle);

    ptr += write_header((void*)ptr, pdulen, flag);
    ptr += write_handle((void*)ptr, handle);

    chat_send(sock, (void*)&pack[0], pdulen, SEND_FLAGS);
}

static void send_broadcast_packet(int sock, char* src_handle, char* text) {
    char pack[MAX_BROADCAST_PACKET_LENGTH_BYTES];
    int pdulen = 0;

    pdulen += write_header((void*)pack, pdulen, FLAG_BROADCAST);
    pdulen += write_handle((void*)(pack + pdulen), src_handle);
    pdulen += write_text((void*) (pack + pdulen), text);

    // Adjust pdu length in header
    ((Chat_header*)(void*)pack)->pdu_length = htons(pdulen);

    chat_send(sock, (void*)pack, pdulen, SEND_FLAGS);
}

static void send_msg_packet(int sock, char* src_handle, uint8_t num_hand, char** handles, char* text) {
    int i, pdulen = 0;
    char pack[MAX_MSG_PACKET_LENGTH_BYTES];
    
    pdulen += write_header((void*)pack, pdulen, FLAG_MSG);
    pdulen += write_handle((void*)(pack + pdulen), src_handle);
    memcpy((void*)(pack + pdulen), (void*)&num_hand, 1);
    pdulen++;

    for(i = 0; i < num_hand; i++) {
        pdulen += write_handle((void*)(pack + pdulen), handles[i]);
    }

    pdulen += write_text((void*)(pack + pdulen), text);
   
    // Adjust pdu length in header 
    ((Chat_header*)(void*)pack)->pdu_length = htons(pdulen);

    chat_send(sock, (void*)pack, pdulen, SEND_FLAGS);
}

static void send_header_num_packet(int sock, uint32_t num, int flag) {
    char pack[MAX_HEAD_NUM_PACKET_LENGTH_BYTES];
    char* ptr = pack;
    
    num = htonl(num);

    ptr += write_header((void*)pack, MAX_HEAD_NUM_PACKET_LENGTH_BYTES, flag);
    memcpy((void*)ptr, (void*)&num, 4);

    chat_send(sock, (void*)&pack[0], MAX_HEAD_NUM_PACKET_LENGTH_BYTES, SEND_FLAGS);
}

// External Send Functions
void send_handle_req(int sock, char* handle) {
    send_header_handle_packet(sock, handle, FLAG_HANDLE_REQ);
}

void send_handle_ack(int sock) {
    send_header_packet(sock, FLAG_HANDLE_ACK);
}

void send_handle_inuse(int sock) {
    send_header_packet(sock, FLAG_HANDLE_INUSE);
}

void send_broadcast(int sock, char* handle, char* text) {
    send_broadcast_packet(sock, handle, text);
}

void send_msg(int sock, char* src_handle, int num_hand, char** handles, char* text) {
    send_msg_packet(sock, src_handle, num_hand, handles, text);
}

void send_dest_handle_err(int sock, char* handle) {
    send_header_handle_packet(sock, handle, FLAG_HANDLE_ERR);
}

void send_exit_req(int sock) {
    send_header_packet(sock, FLAG_EXIT_REQ);
}

void send_exit_ack(int sock) {
    send_header_packet(sock, FLAG_EXIT_ACK);
}

void send_hlist_req(int sock) {
    send_header_packet(sock, FLAG_HLIST_REQ);
}

void send_hlist_num(int sock, uint32_t num) {
    send_header_num_packet(sock, num, FLAG_HLIST_NUM);
}

void send_hlist_entry(int sock, char* handle) {
    send_header_handle_packet(sock, handle, FLAG_HLIST_ENTRY);
}

void send_hlist_end(int sock) {
    send_header_packet(sock, FLAG_HLIST_END);
}

//External Read Functions
void read_broadcast(int sock, char* handle, char* text) {
    read_chat_handle(sock, handle);
    read_chat_text(sock, text);
}

int read_hlist_num(int sock) {
    int numh;

    chat_recv(sock, (void*)&numh, 4, RECV_FLAGS);

    return ntohl(numh);
}
