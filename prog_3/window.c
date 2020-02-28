#include <string.h>
#include <malloc.h>

#include "window.h"

// DOES NOT PROTECT AGAINST SEQUENCE NUMBER OVERFLOW

#define BUF_IS_EMPTY(W) (W->lo == W->curr)
#define IS_WINDOW_CLOSED(W) (W->curr == W->hi)

#define MOVE_LO(W) (W->lo = ((W->lo == W->max) ? W->buf : W->lo + 1))
#define MOVE_HI(W) (W->hi = ((W->hi == W->max) ? W->buf : W->hi + 1))
#define MOVE_CURR(W) (W->curr = ((W->curr == W->max) ? W->buf : W->curr + 1))

#define HIGHEST_WNDW_SEQ(W) (W->lwseq + W->wsize - 1)

#define SEQ_INDEX(W, SEQ) (SEQ - W->lwseq) // gives the number greater than the sequence
#define GET_ENTRY_PTR(W, SEQI) (((W->lo + SEQI) > W->max) ? (W->lo + SEQI - W->wsize - 1) : (W->lo + SEQI))

#define IN_SEQ_RANGE(W, SEQ, USEQ) ((SEQ >= W->lwseq) && (SEQ <= USEQ))

void clear_buf(Bufpacket* buf, int bufsize) {
    while(bufsize--) buf->seq = 0;
}

Window* get_window(int wsize) {
    Window * w;
    
    w = malloc(sizeof(Window));

    w->wsize = wsize;

    w->cseq = 1;
    w->lwseq = 1;

    w->buf = (Bufpacket*)malloc(sizeof(Bufpacket) * (wsize + 1));
    clear_buf(w->buf, wsize + 1);
    w->max = w->buf + w->wsize;

    w->lo = w->buf;
    w->curr = w->buf;
    w->hi = w->buf + wsize;

    return w;
}

// places packet in buffer in sequence order
// returns PACKET_EXISTS if packet is already in buffer, -1 if error, 0 else
int buf_packet(Window* w, uint32_t seq, void* pack, int psize) {
    int seqi;
    Bufpacket* p;

    if (seq == 0) // invalid sequence
        return -1;
    
    if (IN_SEQ_RANGE(w, seq, HIGHEST_WNDW_SEQ(w))) { // within the window's sequence range
        seqi = SEQ_INDEX(w, seq);
        p = GET_ENTRY_PTR(w, seqi);

        if (!IN_SEQ_RANGE(w, seq, w->cseq)) { // move current if not within current window
            if (p->seq != 0) // packet exists
                return PACKET_EXISTS;

            w->cseq = seq + 1;
            w->curr = p;
            MOVE_CURR(w);
        }

        memcpy((void*)p->pack, pack, psize);
    }
    else { // sequence is not within the window's sequence range
        return -1;
    }
}

// gets packet with a given sequence number
// returns NULL if packet does not exist
void* get_packet(Window* w, int seq) {
    int seqi;
    Bufpacket* p;

    if (!IN_SEQ_RANGE(w, seq, w->cseq)) // check if sequence could be in buffer
        return NULL;

    seqi = SEQ_INDEX(w, seq);
    p = GET_ENTRY_PTR(w, seqi);

    if (p->seq == 0) // check if packet exists
        return NULL;

    return (void*)p->pack;
}

// moves window forward once
// returns -1 if error, 0 else
int move_window(Window* w) {
    if (BUF_IS_EMPTY(w))
        return -1;

    w->lwseq++;

    w->lo->seq = 0;

    MOVE_LO(w);
    MOVE_HI(w);

    return 0;
}

// moves window forward n times or until error is hit
// returns -1 if error, 0 else
int move_window_n(Window* w, int n) {
    int ret = 0;

    while(n-- && ((ret = move_window(w)) > 0));

    return ret;
}

bool isWindowClosed(Window* w) {
    return IS_WINDOW_CLOSED(w);
}

void del_window(Window* w) {
    free(w->buf);
    free(w);
}

