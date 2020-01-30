#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "chat_packets.h"

#define RECV_FLAGS MSG_WAITALL
#define SEND_FLAGS 0

#define HANDLE_LENGTH_BYTES 1

int chat_recv(int socket, void* buf, int length, int flags) {
    int out;
    
    if((out = recv(socket, buf, length, flags)) == -1)
        perror("recv call");

    return out;
}

int chat_send(int socket, void* buf, int length, int flags) {
    int out;

    if ((out = send(socket, buf, length, flags)) == -1)
        perror("send call");

    return out;
}

// returns length of remainder of packet
int read_chat_header(int socket, int* pdu_length) {
    int bytes;
    Chat_header head;

    bytes = chat_recv(socket, (void*)&head, sizeof(Chat_header), RECV_FLAGS);

    // socket has closed
    if (bytes == 0)
        return 0;

    *pdu_length = ntohs(head.pdu_length) - sizeof(Chat_header);

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

void send_handle_req(int sock, char* handle) {
    Chat_header head;
    int hlen;
    char pack[MAX_HANDLE_REQ];
    char* ptr = &pack[0];
    
    hlen = strlen(handle);    

    head.pdu_length = sizeof(Chat_header) + hlen + HANDLE_LENGTH_BYTES;
    head.pdu_length = htons(head.pdu_length);
    head.flag = FLAG_HANDLE_REQ;

    memcpy((void*)ptr, (void*)&head, sizeof(Chat_header));
    ptr += sizeof(Chat_header);

    memcpy((void*)ptr, (void*)handle, hlen);

    chat_send(sock, &pack, head.pdu_length, SEND_FLAGS);
}
