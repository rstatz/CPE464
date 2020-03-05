#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/select.h>

#include "debug.h"
#include "cpe464.h"
#include "networks.h"
#include "rcopy_packets.h"

#define MAX_RNAME 1000

typedef enum STATE {TERMINATE = -1,
                    SETUP = 0,
                    SETUP_PARAMS,
                    DATA_RX} STATE;

typedef struct client_args {
    char *from_fname, *to_fname;
    char *rname;
    uint16_t wsize, bsize;
    int port;
    float err;
} client_args;

void usage() {
    fprintf(stderr, "Usage: rcopy from-filename to-filename window-size buffer-size error-percent remote-machine remote-port\n");
    exit(EXIT_FAILURE);
}

void get_args(int argc, char* argv[], client_args* ca) {
    if (argc != 8)
        usage();

    ca->wsize = atoi(argv[3]);
    if (ca->wsize < 1 || ca->wsize > SHRT_MAX) {
        fprintf(stderr, "Invalid window-size\n");
        exit(EXIT_FAILURE);
    }

    ca->bsize = atoi(argv[4]);
    if (ca->bsize < 1 || ca->bsize > (MAX_PACK - sizeof(RC_PHeader))) {
        fprintf(stderr, "Invalid buffer-size\n");
        exit(EXIT_FAILURE);
    }

    ca->err = strtof(argv[5], NULL);
    if (errno != 0)
        usage();
    if ((ca->err > 100) || (ca->err < 0)) {
        fprintf(stderr, "Invalid error-percent (0 to 100)\n");       
        usage();
    }
   
    ca->port = atoi(argv[7]);
    if (ca->port < 1) {
        fprintf(stderr, "Invalid port number ");
        exit(EXIT_FAILURE);
    }
        
    if ((strlen(argv[1]) > MAX_FNAME) || (strlen(argv[2]) > MAX_FNAME)) {
        fprintf(stderr, "Filenames can be a maximum of %d characters long\n", MAX_FNAME);
        exit(EXIT_FAILURE);
    }

    if (strlen(argv[6]) > MAX_RNAME) {
        fprintf(stderr, "Remote host name can be a maximum of %d characters long\n", MAX_RNAME);
        exit(EXIT_FAILURE);
    }

    ca->from_fname = argv[1];
    ca->to_fname = argv[2];
    ca->rname = argv[6];
}

static STATE FSM_setup(client_args* ca, int* sock, UDPInfo* udp) {
    uint8_t setup_pack[sizeof(RC_PHeader)];
    uint8_t setup_ack_pack[sizeof(RC_PHeader)];

    // setup client socket
    *sock = setupUdpClientToServer(udp, ca->rname, ca->port);   
    
    // setup packet
    build_setup_pack((void*)setup_pack);

    if (select_resend_n(*sock, TIMEOUT_VALUE_S, 0, USE_TIMEOUT, MAX_ATTEMPTS, udp) == TIMEOUT_REACHED) {
        return TERMINATE;
    }

    // check recv flag
    switch(recv_rc_pack((void*)&setup_ack_pack, MAX_PACK, *sock, udp)) {
        case(FLAG_SETUP_ACK) :
            return SETUP_PARAMS;
        default:
            DEBUG_PRINT("rcopy: expected setup ack, received unknown packet\n");
            return TERMINATE;
    }
}

static STATE FSM_setup_params(client_args* ca, int sock, UDPInfo* udp) {
    uint8_t sp_pack[MAX_PACK];
    uint8_t sp_ack_pack[sizeof(RC_PHeader)];  

    build_setup_params_pack((void*)&sp_pack, ca->wsize, ca->bsize, ca->from_fname);

    if (select_resend_n(sock, TIMEOUT_VALUE_S, 0, USE_TIMEOUT, MAX_ATTEMPTS, udp) == TIMEOUT_REACHED) {
        return TERMINATE;
    }

    // check recv flag
    switch(recv_rc_pack((void*)&sp_ack_pack, MAX_PACK, sock, udp)) {
        case(FLAG_SETUP_PARAMS_ACK) :
            DEBUG_PRINT("rcopy: filenames accepted\n");
            return DATA_RX;
        case(FLAG_BAD_FNAME) :
            fprintf(stderr, "rcopy: bad filename\n");
            return TERMINATE;
        default:
            DEBUG_PRINT("rcopy: expected setup param response, received unknown packet\n");
            return TERMINATE;
    }
}

static void start_rcopy(client_args* ca) {
    STATE state = SETUP;
    UDPInfo udp = {0};
    int client_sock = 0; 
    bool run = true;
    
    while(run) {
        switch(state) {
            case(SETUP) :
                state = FSM_setup(ca, &client_sock, &udp);
                break;
            case(SETUP_PARAMS) :
                state = FSM_setup_params(ca, client_sock, &udp);
                state = TERMINATE; // TODO for testing
                break;
            case(DATA_RX) :
                break;
            case(TERMINATE) :
                close(client_sock);
                run = false;
                break;
            default:
                DEBUG_PRINT("rcopy FSM: bad state\n");
                state = TERMINATE;
                break;
        }
    } 
}

int main(int argc, char* argv[]) {
    client_args ca;

    #ifdef DEBUG
        setvbuf(stdout, NULL, _IONBF, 0);
    #endif

    get_args(argc, argv, &ca);

    sendErr_init(ca.err, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON); // TODO debug off

    start_rcopy(&ca);

    exit(EXIT_SUCCESS);
}
