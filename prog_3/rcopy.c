#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/select.h>

#include "debug.h"
#include "cpe464.h"
#include "window.h"
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

    FILE* write_fd;

    int maxrr;
} client_args;

void usage() {
    fprintf(stderr, "Usage: rcopy from-filename to-filename window-size buffer-size error-percent remote-machine remote-port\n");
    exit(EXIT_FAILURE);
}

void get_args(int argc, char* argv[], client_args* ca) {
    ca->write_fd = NULL;

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
    uint8_t tries = MAX_ATTEMPTS;
    bool done = false;
    int psize;

    // check write file
    if ((ca->write_fd = fopen(ca->to_fname, "w")) < 0) {
        fprintf(stderr, "rcopy: Invalid output filepath\n");
        return TERMINATE;
    }

    // setup client socket
    *sock = setupUdpClientToServer(udp, ca->rname, ca->port);   
    udp->sock = *sock;   
 
    // setup packet
    build_setup_pack((void*)setup_pack);

    while(!done) {
        done = true; // assume success

        if ((tries = select_resend_n(*sock, TIMEOUT_VALUE_S, 0, USE_TIMEOUT, tries, udp)) == TIMEOUT_REACHED)
            return TERMINATE;

        // check recv flag
        switch(recv_rc_pack((void*)setup_ack_pack, MAX_PACK, &psize, *sock, udp)) {
            case(FLAG_SETUP_ACK) :
                return SETUP_PARAMS;
            case(CRC_ERROR) :
                DEBUG_PRINT("rcopy: CRC Error SETUP\n");
            default:
                DEBUG_PRINT("rcopy: expected setup ack, received unknown packet\n");
                done = (tries == 0);
                break;
        }
    }

    return TERMINATE;
}

static STATE FSM_setup_params(client_args* ca, Window** w, int sock, UDPInfo* udp) {
    uint8_t sp_pack[MAX_PACK];
    uint8_t sp_ack_pack[MAX_PACK];
    uint8_t tries = MAX_ATTEMPTS;
    bool done = false;
    int psize;
    
    build_setup_params_pack((void*)sp_pack, ca->wsize, ca->bsize, ca->from_fname);
    
    while (!done) {
        done = true; //assume success

        if ((tries = select_resend_n(sock, TIMEOUT_VALUE_S, 0, USE_TIMEOUT, tries, udp)) == TIMEOUT_REACHED)
            return TERMINATE;

        switch(recv_rc_pack((void*)sp_ack_pack, MAX_PACK, &psize, sock, udp)) {
            case(FLAG_SETUP_PARAMS_ACK) :
                DEBUG_PRINT("rcopy: to filename accepted\n");
                // Pre RX setup
                *w = get_window(ca->wsize);
                ca->maxrr = 1;
                return DATA_RX;
            case(FLAG_BAD_FNAME) :
                fprintf(stderr, "rcopy: Invalid input filepath\n");
                return TERMINATE;
            case(CRC_ERROR) :
                DEBUG_PRINT("rcopy: CRC Error SETUP_PARAMS\n");
            default:
                DEBUG_PRINT("rcopy: expected setup param response, received bad packet\n");
                done = (tries == 0);
                break;
        }
    }

    return TERMINATE;
}

static void send_srej(int seq, UDPInfo* udp) {
    uint8_t srej_pack[RC_SREJ_SIZE];

    //DEBUG_PRINT("rcopy: sending SREJ %d\n", seq);

    build_srej_pack((void*)srej_pack, seq);
    send_last_rc_build(udp->sock, udp);
}

static void process_rx(client_args* ca, Window* w, void* new_data_pack, int psize, UDPInfo* udp) {
    int dsize;
    void *write_data_pack, *write_data; 
    
    buf_packet(w, RCSEQ(new_data_pack), new_data_pack, psize);

    // determine if SREJ needs to be sent
    if (RCSEQ(new_data_pack) > ca->maxrr) {
        DEBUG_PRINT("rcopy: srej detected, rcv seq %d, %d\n", RCSEQ(new_data_pack), psize);
        send_srej(ca->maxrr, udp);
    }

    // write any available data
    while ((write_data_pack = get_lowest_packet(w, &psize)) != NULL) {
        write_data = parse_data_pack(write_data_pack, psize, &dsize);

        move_window_n(w, 1); // move window forward once

        DEBUG_PRINT("rcopy: writing data seq %d, psize %d\n", RCSEQ(write_data_pack), psize);

        if (fwrite(write_data, 1, dsize, ca->write_fd) < dsize) {
            DEBUG_PRINT("rcopy: failed to write all data\n");
        }

        ca->maxrr = RCSEQ(write_data_pack) + 1;
    }
}

static STATE FSM_data_rx(client_args* ca, Window* w, int sock, UDPInfo* udp) {  
    uint8_t rr_pack[RC_RR_SIZE];
    uint8_t rx_pack[MAX_PACK];
    uint8_t eof_ack_pack[RC_EOF_ACK_SIZE];
    uint8_t tries = MAX_ATTEMPTS;
    int psize;

    //DEBUG_PRINT("rcopy: sending rr %d\n", ca->maxrr);
    build_rr_pack((void*)rr_pack, ca->maxrr);

    if ((select_resend_n(sock, TIMEOUT_VALUE_S, 0, USE_TIMEOUT, tries, udp)) == TIMEOUT_REACHED)
        return TERMINATE;

    switch(recv_rc_pack((void*)rx_pack, MAX_PACK, &psize, sock, udp)) {
        case(FLAG_DATA) :
            process_rx(ca, w, (void*)rx_pack, psize, udp);
            break;
        case(FLAG_EOF) :
            build_eof_ack_pack((void*)eof_ack_pack);
            send_last_rc_build(sock, udp);
            return TERMINATE;
        case(CRC_ERROR) :
            //send_srej(ca->maxrr, udp);
            break;
        default:
            DEBUG_PRINT("rcopy: received bad packet in DATA_RX\n");
            break;
    }
    
    return DATA_RX;
}

static void start_rcopy(client_args* ca) {
    STATE state = SETUP;
    UDPInfo udp = {{0}};
    Window* w = NULL;
    int client_sock = 0; 
    bool run = true;
    
    while(run) {
        switch(state) {
            case(SETUP) :
                DEBUG_PRINT("rcopy: sending connection request\n");
                state = FSM_setup(ca, &client_sock, &udp);
                break;
            case(SETUP_PARAMS) :
                DEBUG_PRINT("rcopy: sending setup params\n");
                state = FSM_setup_params(ca, &w, client_sock, &udp);
                break;
            case(DATA_RX) :
                //DEBUG_PRINT("rcopy: starting data Rx\n");
                state = FSM_data_rx(ca, w, client_sock, &udp);
                break;
            case(TERMINATE) :
                if (ca->write_fd != NULL) fclose(ca->write_fd);
                if (w != NULL) del_window(w);
                close(client_sock);
                run = false;
                DEBUG_PRINT("rcopy: sayonara\n");
                break;
            default:
                DEBUG_PRINT("rcopy FSM: bad state\n");
                state = TERMINATE;
                break;
        }
    } 
}

int main(int argc, char* argv[]) {
    client_args ca = {0};

    #ifdef DEBUG
        setvbuf(stdout, NULL, _IONBF, 0);
    #endif

    get_args(argc, argv, &ca);

    sendErr_init(ca.err, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON); // TODO debug off

    start_rcopy(&ca);

    exit(EXIT_SUCCESS);
}
