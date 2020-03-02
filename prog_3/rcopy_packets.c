#include <stdint.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "checksum.h"
#include "networks.h"
#include "rcopy_packets.h"

#define VALID_CHECKSUM 0

static int build_rc_header(uint32_t seq, int flag, RC_PHeader* head) {
    head->seq = htonl(seq);
    head->crc = 0;
    head->flag = flag;

    return sizeof(RC_PHeader);
}

// build places packet in buffer and returns length of packet
int build_setup_pack(void* buf) {
    return build_rc_header(0, FLAG_SETUP, (RC_PHeader*)buf);
}

int build_setup_ack_pack(void* buf) {
    return build_rc_header(0, FLAG_SETUP_ACK, (RC_PHeader*)buf);
}

int build_data_pack(void* buf, uint32_t seq, void* data, int len) {
    RC_PHeader* head = (RC_PHeader*)buf;

    build_rc_header(seq, FLAG_DATA, head);

    memcpy((void*)(((RC_PHeader*)buf) + 1), data, len);

    return len + sizeof(RC_PHeader);
}

int build_rr_pack(void* buf, uint32_t seq) {
    return build_rc_header(0, FLAG_RR, (RC_PHeader*)buf);
}

int build_srej_pack(void* buf, uint32_t seq) {
    return build_rc_header(seq, FLAG_SREJ, (RC_PHeader*)buf);
}

int build_setup_params_pack(void* buf, uint16_t wsize, uint16_t bsize, char* fname) {
    RC_Param_Pack* pack = (RC_Param_Pack*)buf;

    build_rc_header(0, FLAG_SETUP_PARAMS, &pack->head);

    pack->wsize = htons(wsize);
    pack->bsize = htons(bsize);

    strcpy((char*)(pack + 1), fname);

    return sizeof(RC_Param_Pack) + strlen(fname) + 1;
}

int build_setup_params_ack_pack(void* buf) {
    return build_rc_header(0, FLAG_SETUP_PARAMS_ACK, (RC_PHeader*)buf);
}

int build_eof_pack(void* buf, uint32_t seq) {
    return build_rc_header(seq, FLAG_EOF, (RC_PHeader*)buf);
}

int build_eof_ack_pack(void* buf) {
    return build_rc_header(0, FLAG_EOF_ACK, (RC_PHeader*)buf);
}

int build_bad_fname_pack(void* buf) {
    return build_rc_header(0, FLAG_BAD_FNAME, (RC_PHeader*)buf);
}

// returns length of sent packet
int send_rc_pack(void* buf, int len, int sock, UDPInfo* udp) {
    // calculate crc
    ((RC_PHeader*)buf)->crc = -1 * in_cksum(buf, len);

    // TODO sendtoerr packet

    // return length
}

// only returns data from a single datagram no matter buffer length
// returns flag of packet, CRC_ERROR if error, -1 if other error
int recv_rc_pack(void* buf, int len, int sock, UDPInfo* udp) {
    len = (len < MAX_PACK) ? len : MAX_PACK;

    len = safeRecvfrom(sock, buf, MAX_PACK, UDP_RECV_FLAGS, &udp->addr, &udp->addr_len);

    if (len < sizeof(RC_PHeader))
        return -1;

    if (in_cksum(buf, len) != VALID_CHECKSUM)
        return CRC_ERROR;

    return ((RC_PHeader*)buf)->flag;
}

void parse_setup_params(void* buf, uint16_t* wsize, uint16_t* bsize, char* fname) {
    RC_Param_Pack* pack;

    *wsize = ntohs(pack->wsize);
    *bsize = ntohs(pack->bsize);

    strcpy(fname, (char*)(pack + 1));
}
