#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>
#include <stdbool.h>

#include "rcopy_packets.h"

#define PACKET_EXISTS -2

typedef struct Bufpacket {
    uint32_t seq;
    uint8_t pack[MAX_PACK];
} Bufpacket;

typedef struct Window {
    uint32_t wsize;

    uint32_t cseq; // highest current sequence
    uint32_t lwseq; // lowest sequence allowed by window

    Bufpacket* lo;
    Bufpacket* curr;
    Bufpacket* hi; 

    Bufpacket* max;
    Bufpacket* buf;
} Window;

Window* get_window(int bsize);

int buf_packet(Window*, uint32_t, void*, int);
void* get_packet(Window*, uint32_t);
int move_window(Window*);
int move_window_n(Window*, int);

bool isWindowClosed(Window*);

void del_window(Window*);

#endif
