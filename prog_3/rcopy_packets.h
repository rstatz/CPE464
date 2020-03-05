#ifndef RCOPY_PACKETS_H
#define RCOPY_PACKETS_H

#include <stdint.h>

typedef struct RC_PHeader {
    uint32_t seq; // network order
    uint32_t crc; // network order
    uint8_t flag;
} __attribute__((packed)) RC_PHeader;

#define MAX_ATTEMPTS 10

#define MAX_PACK 1400
#define MAX_FNAME 200
//#define MAX_WSIZE 
//#define MAX_BSIZE 

#define CRC_ERROR -2

#define TIMEOUT_VALUE_S 1
#define TIMEOUT_REACHED true

bool select_resend_n(int sock, int seconds, int microseconds, bool set_null, int num_tries, UDPInfo*);

int send_rc_pack(void* buf, int len, int sock, UDPInfo*);
int recv_rc_pack(void* buf, int len, int sock, UDPInfo*);

// Flag = 1 : Setup packet (rcopy to server)
// | header |
#define FLAG_SETUP 1
int build_setup_pack(void*);
//int send_rc_setup_pack(void*, int, UDPInfo*);

// Flag = 2 : Setup response packet (server to rcopy)
// | header |
#define FLAG_SETUP_ACK 2
int build_setup_ack_pack(void*);

// Flag = 3 : Data Packet
// | header | data |
#define FLAG_DATA 3
int build_data_pack(void* buf, uint32_t seq, void* data, int len); 

// Flag = 5 : RR
// | header |
#define FLAG_RR 5
int build_rr_pack(void*, uint32_t seq);

// Flag = 6 : SREJ
// | header |
#define FLAG_SREJ 6
int build_srej_pack(void*, uint32_t seq);

// Flag = 7 : Setup parameters (rcopy to server)
// | header | window size | buffer size | filename |
#define FLAG_SETUP_PARAMS 7
typedef struct RC_Param_Pack {
    RC_PHeader head;
    
    uint16_t wsize; // network order
    uint16_t bsize; // network order

    // start filename (null terminated)
} __attribute__((packed)) RC_Param_Pack;

int build_setup_params_pack(void*, uint16_t wsize, uint16_t bsize, char* fname);
//int send_rc_setup_params_pack(void*, int sock, UDPInfo* udp);
void parse_setup_params(void*, uint16_t*, uint16_t*, char*);

// Flag 8 : Setup parameter response (server to rcopy)
// | header |
#define FLAG_SETUP_PARAMS_ACK 8
int build_setup_params_ack_pack(void*);

// Flag 9 : EOF (server to rcopy)
// | header |
#define FLAG_EOF 9
int build_eof_pack(void*, uint32_t seq);

// Flag 10 : EOF_ACK (rcopy to server)
// | header |
#define FLAG_EOF_ACK 10
int build_eof_ack_pack(void*);

// Flag 11 : Bad filename
// | header |
#define FLAG_BAD_FNAME 11
int build_bad_fname_pack(void*);

#endif
