#include <stdint.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "cpe464.h"
//#include "checksum.h"
#include "debug.h"
#include "networks.h"
#include "rcopy_packets.h"

#define VALID_CHECKSUM 0

void* g_last_pack;
int g_len;

static int build_rc_header(uint32_t seq, int flag, RC_PHeader* head) {
    head->seq = htonl(seq);
    head->crc = 0;
    head->flag = flag;

    return sizeof(RC_PHeader);
}

// build places packet in buffer and returns length of packet
int build_setup_pack(void* buf) {
    g_last_pack = buf;
    g_len = build_rc_header(0, FLAG_SETUP, (RC_PHeader*)buf);

    return g_len;
}

int build_setup_ack_pack(void* buf) {
    g_last_pack = buf;   
    g_len = build_rc_header(0, FLAG_SETUP_ACK, (RC_PHeader*)buf);

    return g_len;
}

int build_data_pack(void* buf, uint32_t seq, void* data, int len) {
    RC_PHeader* head = (RC_PHeader*)buf;

    build_rc_header(seq, FLAG_DATA, head);

    memcpy((void*)(((RC_PHeader*)buf) + 1), data, len);

    g_last_pack = buf;
    g_len = len + sizeof(RC_PHeader);

    return g_len;
}

int build_rr_pack(void* buf, uint32_t seq) {
    g_last_pack = buf;
    g_len = build_rc_header(0, FLAG_RR, (RC_PHeader*)buf);

    return g_len;
}

int build_srej_pack(void* buf, uint32_t seq) {
    g_last_pack = buf;
    g_len = build_rc_header(seq, FLAG_SREJ, (RC_PHeader*)buf);

    return g_len;
}

int build_setup_params_pack(void* buf, uint16_t wsize, uint16_t bsize, char* fname) {
    RC_Param_Pack* pack = (RC_Param_Pack*)buf;

    build_rc_header(0, FLAG_SETUP_PARAMS, &pack->head);

    pack->wsize = htons(wsize);
    pack->bsize = htons(bsize);

    strcpy((char*)(pack + 1), fname);

    g_last_pack = buf;
    g_len = sizeof(RC_Param_Pack) + strlen(fname) + 1;

    return g_len;
}

int build_setup_params_ack_pack(void* buf) {
    g_last_pack = buf;
    g_len = build_rc_header(0, FLAG_SETUP_PARAMS_ACK, (RC_PHeader*)buf);

    return g_len;
}

int build_eof_pack(void* buf, uint32_t seq) {
    g_last_pack = buf;
    g_len = build_rc_header(seq, FLAG_EOF, (RC_PHeader*)buf);

    return g_len;
}

int build_eof_ack_pack(void* buf) {
    g_last_pack = buf;
    g_len = build_rc_header(0, FLAG_EOF_ACK, (RC_PHeader*)buf);

    return g_len;
}

int build_bad_fname_pack(void* buf) {
    g_last_pack = buf;
    g_len = build_rc_header(0, FLAG_BAD_FNAME, (RC_PHeader*)buf);

    return g_len;
}

// returns length of sent packet
int send_rc_pack(void* buf, int len, int sock, UDPInfo* udp) {
    // calculate crc
    ((RC_PHeader*)buf)->crc = -1 * in_cksum((unsigned short*)buf, len);

    // send packet and return length
    return safeSendtoErr(sock, buf, len, udp);
}

int send_rc_last(int sock, UDPInfo* udp) {
    return send_rc_pack(g_last_pack, g_len, sock, udp);
}

/*int send_rc_setup_pack(void* buf, int sock, UDPInfo* udp) {
    return send_rc_pack(buf, sizeof(RC_PHeader), sock, udp);
}

int send_rc_setup_params_pack(void* buf, int sock, UDPInfo* udp) {
    int len = 0;

    len+=sizeof(RC_Param_Pack);
    len+=strlen((char*)((RC_Param_Pack*)buf + 1));

    return send_rc_pack(buf, len, sock, udp);
}*/

// only returns data from a single datagram no matter buffer length
// returns flag of packet, CRC_ERROR if error, -1 if other error
int recv_rc_pack(void* buf, int len, int sock, UDPInfo* udp) {
    len = (len < MAX_PACK) ? len : MAX_PACK;

    len = safeRecvfrom(sock, buf, MAX_PACK, udp);

    if (len < sizeof(RC_PHeader))
        return -1;

    if (in_cksum((unsigned short*)buf, len) != VALID_CHECKSUM)
        return CRC_ERROR;

    return ((RC_PHeader*)buf)->flag;
}

void parse_setup_params(void* buf, uint16_t* wsize, uint16_t* bsize, char* fname) {
    RC_Param_Pack* pack = (RC_Param_Pack*)buf;

    *wsize = ntohs(pack->wsize);
    *bsize = ntohs(pack->bsize);

    strcpy(fname, (char*)(pack + 1));
}

// returns select times out n times, false otherwise
bool select_resend_n(int sock, int32_t seconds, int microseconds, bool set_null, int num_tries, UDPInfo* udp) {
    bool timed_out = false;

    do {
        send_rc_last(sock, udp);
        
        timed_out = select_call(sock, seconds, microseconds, set_null); 
    } while (timed_out && --num_tries > 0);

    // terminate if max attempts reached
    if (timed_out == true) {
        DEBUG_PRINT("rcopy: select timeout reached\n");
        return true;
    }

    return false;
}
