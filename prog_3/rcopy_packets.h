#ifndef RCOPY_PACKETS_H
#define RCOPY_PACKETS_H

#include <stdint.h>

typedef struct RC_PHeader {
    uint32_t seq; // network order
    uint32_t crc; // network order
    uint8_t flag;
} RC_PHeader;

#define MAX_PACK 1400

// Flag = 1 : Setup packet (rcopy to server)
// | header |
#define FLAG_SETUP 1
void send_setup(int sock);

// Flag = 2 : Setup response packet (server to rcopy)
// | header |
#define FLAG_SETUP_RESP 2
void send_setup_resp(int sock);

// Flag = 3 : Data Packet
// | header | data |
#define FLAG_DATA 3
void send_data(int sock, int seq, int len, void* data); 

// Flag = 5 : RR
// | header |
#define FLAG_RR 5
void send_rr(int sock, int seq);

// Flag = 6 : SREJ
// | header |
#define FLAG_SREJ 6
void send_srej(int sock, int seq);

// Flag = 7 : Setup parameters (rcopy to server)
// | header | window size | buffer size | filename |
#define FLAG_SETUP_PARAMS 7
void send_setup_params(int sock, int wsize, int bsize, char* fname);

// Flag 8 : Setup parameter response (server to rcopy)
// | header |
#define FLAG_SETUP_PARAMS_RESP 8
void send_setup_params_resp(int sock);

// Flag 9 : EOF (server to rcopy)
// | header |
#define FLAG_EOF 9
void send_eof(int sock);

// Flag 10 : EOF_ACK (rcopy to server)
// | header |
#define FLAG_EOF_ACK 10
void send_eof_ack(int sock);

// Flag 11 : Bad filename
// | header |
#define FLAG_BAD_FILENAME 11
void send_bad_fname(int sock);

#endif
