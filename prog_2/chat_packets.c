#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "chat_packets.h"

#define RECV_FLAGS MSG_WAITALL
#define SEND_FLAGS 0

#define HANDLE_LENGTH_BYTES 1

#define MAX_HEAD_HANDLE_LENGTH_BYTES (101 + sizeof(Chat_header))
#define MAX_HEAD_NUM_PACKET_LENGTH_BYTES (4 + sizeof(Chat_header))
#define MAX_MSG_PACKET_LENGTH_BYTES (806 + sizeof(Chat_header))

static int chat_recv(int socket, void* buf, int length, int flags) {
    int out;
    
    if((out = recv(socket, buf, length, flags)) == -1)
        perror("recv call");

    return out;
}

static int chat_send(int socket, void* buf, int length, int flags) {
    int out;

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

    *pdu_length = htons(head.pdu_length) - sizeof(Chat_header);

    return head.flag;
}

// hbuf must be of at least size 101 bytes
void read_chat_handle(int socket, char* hbuf) {
    int length;

    chat_recv(socket, (void*)&length, HANDLE_LENGTH_BYTES, RECV_FLAGS);
    chat_recv(socket, (void*)hbuf, length, RECV_FLAGS);

    hbuf[length] = '\0';
}

void trash_packet(int socket, int length) {
    chat_recv(socket, NULL, length, RECV_FLAGS);
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
    int tlen = strlen(text) + 1;

    memcpy(pack, (void*)text, tlen);

    return tlen;
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

// pass in list of handles with source handle first
static void send_msg_packet(int sock, int num_hand, char** handles, char* text, int flag) {
    int i, pdulen = sizeof(Chat_header);
    char pack[MAX_MSG_PACKET_LENGTH_BYTES];
    char* ptr = &pack[0] + sizeof(Chat_header);

    for(i = 0; i < num_hand; i++) {
        pdulen += write_handle((void*)ptr, handles[i]);
        ptr = &pack[0] + pdulen;
    }

    pdulen += write_text((void*)ptr, text);

    write_header((void*)&pack[0], pdulen, SEND_FLAGS);

    chat_send(sock, (void*)&pack[0], pdulen, SEND_FLAGS);
}

static void send_header_num_packet(int sock, uint32_t num, int flag) {
    char pack[MAX_HEAD_NUM_PACKET_LENGTH_BYTES];
    char* ptr = &pack[0];

    ptr+= write_header((void*)&pack[0], MAX_HEAD_NUM_PACKET_LENGTH_BYTES, SEND_FLAGS);
    
    num = htonl(num);

    memcpy((void*)ptr, &num, 4);

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
    send_msg_packet(sock, 1, &handle, text, FLAG_BROADCAST);
}

void send_msg(int sock, int num_hand, char** handles, char* text) {
    send_msg_packet(sock, num_hand, handles, text, FLAG_MSG);
}

void send_handle_err(int sock, char* handle) {
    send_header_packet(sock, FLAG_HANDLE_ERR);
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
