#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/select.h>

#include "debug.h"
#include "cpe464.h"
#include "window.h"
#include "networks.h"
#include "rcopy_packets.h"
#include "gethostbyname.h"

#define NEW_CLIENT_TIMEOUT_S 10

#define TFER_DONE(SP) (SP->maxrr == SP->seq)

#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))

#ifdef DEBUG
#include "test_rcopy.h"
#endif

typedef enum STATE {TERMINATE=-1,
                    SETUP = 0,
                    SETUP_ACK,
                    GET_PARAMS,  
                    DATA_TX, 
                    DATA_RECOV, // still needed with current window implementation?
                    SEND_EOF,
                    SERVER_PROCESS} STATE;

typedef struct SParams {
    uint16_t wsize, bsize;
    uint32_t seq;

    float err;
    int port;
    
    uint32_t srej_seq;
    uint32_t maxrr; // maximum rr received to date

    FILE* read_fd;
} SParams;

static void init_sp(SParams* sp) {
    sp->seq = 1;
    sp->maxrr = 0;
    sp->read_fd = NULL;
}

static void usage() {
    fprintf(stderr, "Usage: server error-percent [port-number]\n");
    exit(EXIT_FAILURE);
}

static void get_args(int argc, char* argv[], SParams* sp) {
    if (argc > 3 || argc < 2)
        usage();
    
    sp->err = strtof(argv[1], NULL);
    if (errno != 0)
        usage();
    if ((sp->err > 100) || (sp->err < 0)) {
        fprintf(stderr, "Invalid error-percent (0 to 100)\n");       
        usage();
    }

    sp->port = (argc == 3) ? atoi(argv[2]) : 0;
}

STATE FSM_setup_client(int* sock, SParams* sp) {
    pid_t pid;
    
    if ((pid = fork()) < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    
    if (pid != 0) { // server
        return SERVER_PROCESS;
    }

    // child
    sendErr_init(sp->err, DROP_ON, FLIP_ON, 
        #ifdef DEBUG 
            DEBUG_ON, 
        #else
            DEBUG_OFF, 
        #endif
            RSEED_ON);

    // close server socket?
    
    // get new client socket
    *sock = get_UDP_socket();

    return SETUP_ACK;
}

STATE FSM_setup_ack(int sock, void* pack, UDPInfo* udp) {    
    uint8_t setup_ack_pack[MAX_PACK];
    uint8_t tries = MAX_ATTEMPTS;
    bool done = false;
    int psize;

    build_setup_ack_pack((void*)setup_ack_pack); 

    while(!done) {
        done = true; // assume success

        if ((tries = select_resend_n(sock, TIMEOUT_VALUE_S, 0, USE_TIMEOUT, tries, udp)) == TIMEOUT_REACHED)
            return TERMINATE;

        switch(recv_rc_pack(pack, MAX_PACK, &psize, sock, udp)) {
            case(FLAG_SETUP_PARAMS) :
                DEBUG_PRINT("server: received setup params\n");
                return GET_PARAMS;
            case(CRC_ERROR) :
                DEBUG_PRINT("server: CRC Error SETUP_ACK\n");
            default:
                DEBUG_PRINT("server: expected setup param packet, received bad packet\n");
                done = (tries == 0);
                break;
        }
    }
    
    return TERMINATE;
}

static STATE FSM_get_params(int sock, void* setup_params_pack, Window** w, UDPInfo* udp, SParams* sp) {
    uint8_t response_pack[MAX_PACK];
    uint8_t start_tx_pack[MAX_PACK];
    int psize;
    char* fname;

    uint8_t tries = MAX_ATTEMPTS;
    bool done = false;
 
    parse_setup_params(setup_params_pack, &sp->wsize, &sp->bsize, &fname);

    DEBUG_PRINT("RCOPY CONFIG VALUES:\n");
    DEBUG_PRINT("    FNAME=%s\n", fname);
    DEBUG_PRINT("    WSIZE=%d\n", sp->wsize);
    DEBUG_PRINT("    BSIZE=%d\n", sp->bsize);
    // TODO check these values server side?

    // check to from file exists
    if (access(fname, R_OK|F_OK) == -1) {
        DEBUG_PRINT("server: bad filename received, sending response\n");
        build_bad_fname_pack((void*)response_pack);
        send_last_rc_build(sock, udp);
        return TERMINATE;
    }

    // open from file
    sp->read_fd = fopen(fname, "r");

    // set up window
    *w = get_window(sp->wsize);

    DEBUG_PRINT("server: good filename received, sending response\n");
    build_setup_params_ack_pack((void*)response_pack);

    // send filename_ack
    while (!done) {
        done = true; // assume success
        
        if ((tries = select_resend_n(sock, TIMEOUT_VALUE_S, 0, USE_TIMEOUT, tries, udp)) == TIMEOUT_REACHED)
            return TERMINATE;

        switch(recv_rc_pack((void*)start_tx_pack, MAX_PACK, &psize, sock, udp)) {
            case(FLAG_RR) :
                DEBUG_PRINT("server: received RR 1\n"); // TODO check if 1?
                return DATA_TX;
            case(CRC_ERROR) :
                DEBUG_PRINT("server: CRC Error GET_PARAMS\n");
            default:
                DEBUG_PRINT("server: expected RR1, received bad packet\n");
                done = (tries == 0);
                break;
        }
    }

    return TERMINATE;
}

static STATE process_rx(int sock, Window* w, UDPInfo* udp, SParams* sp) {
    uint8_t rx_pack[MAX_PACK];
    int psize;
    void* pack;

    switch(recv_rc_pack((void*)rx_pack, MAX_PACK, &psize, sock, udp)) {
        case(FLAG_RR) :
            //DEBUG_PRINT("server: RCVD RR %d\n", RCSEQ(rx_pack));
            if (sp->maxrr == RCSEQ(rx_pack)) {
                // received duplicate rr
                if((pack = get_packet(w, RCSEQ(rx_pack), &psize)) != NULL)
                    send_rc_pack(pack, psize, sock, udp); 
            } else {
                move_window_seq(w, RCSEQ(rx_pack));
                sp->maxrr = MAX(RCSEQ(rx_pack), sp->maxrr);
            }
            //DEBUG_PRINT("maxrr = %d\n", sp->maxrr);
            break;
        case(FLAG_SREJ) :
            //DEBUG_PRINT("server: RCVD SREJ %d\n", RCSEQ(rx_pack));
            sp->srej_seq = RCSEQ(rx_pack);
            return DATA_RECOV;
        case(CRC_ERROR) :
            DEBUG_PRINT("server: CRC Error DATA TX\n");
        default:
            DEBUG_PRINT("server: received bad packet\n");
            break;
    }

    return DATA_TX;
}

static STATE FSM_data_tx(int sock, Window* w, UDPInfo* udp, SParams* sp) {
    void* probe_pack;
    
    uint8_t tx_pack[MAX_PACK];
    uint8_t data[MAX_PACK];

    int dlen, psize, timeout;
    static bool eof = false;

    while (!isWindowClosed(w) && !eof) { 
        // read from disk
        if (((dlen = fread((void*)data, 1, sp->bsize, sp->read_fd)) < sp->bsize)) {
            DEBUG_PRINT("server: reached eof\n");
            
            eof = true;
        }
        
        // buffer and send packet
        psize = build_data_pack((void*)tx_pack, sp->seq, (void*)data, dlen);
        buf_packet(w, sp->seq, (void*)tx_pack, psize);
        DEBUG_PRINT("server: sending data seq %d, psize %d\n", sp->seq, psize); 
        sp->seq++;
        send_rc_pack((void*)tx_pack, psize, sock, udp);
    
        if(eof)
            break;
        
        // receive all packets
        while(!select_call(sock, 0, 0, USE_TIMEOUT)) {
            if (process_rx(sock, w, udp, sp) == DATA_RECOV)
                return DATA_RECOV;
        }
    }

    while (isWindowClosed(w) || eof) {
        timeout = MAX_ATTEMPTS;

        while (select_call(sock, TIMEOUT_VALUE_S, 0, USE_TIMEOUT) && (timeout > 0)) {
            // may just inch forward when it gets here, but its a rare case
            probe_pack = get_lowest_packet(w, &psize);
            send_rc_pack(probe_pack, psize, sock, udp);
            timeout--;
        }
        
        if (timeout == 0)
            return TERMINATE;
        
        if (process_rx(sock, w, udp, sp) == DATA_RECOV)
            return DATA_RECOV; 

        if (eof && TFER_DONE(sp))
            return SEND_EOF; 
    }

    return DATA_TX;
}

static STATE FSM_data_recov(int sock, Window* w, UDPInfo* udp, SParams* sp) {
    void* pack;
    int psize;
    int timeout = MAX_ATTEMPTS - 1;
    bool timed_out = false;

    pack = get_packet(w, sp->srej_seq, &psize);

    do {
        send_rc_pack(pack, psize, sock, udp); 
    } while((timed_out = select_call(sock, TIMEOUT_VALUE_S, 0, USE_TIMEOUT)) && (timeout-- > 0));

    if (timed_out)
        return TERMINATE;
    
    return process_rx(sock, w, udp, sp);
}

static STATE FSM_send_eof(int sock, UDPInfo* udp, SParams* sp) {
    uint8_t pack[MAX_PACK];
    uint8_t eof_pack[RC_EOF_SIZE];
    static int timeout = MAX_ATTEMPTS;
    int psize;

    build_eof_pack((void*)eof_pack);
   
    if (select_resend_n(sock, TIMEOUT_VALUE_S, 0, USE_TIMEOUT, timeout, udp) == TIMEOUT_REACHED)
        return TERMINATE;
 
    switch(recv_rc_pack((void*)pack, MAX_PACK, &psize, sock, udp)) {
        case(FLAG_EOF_ACK) :
            return TERMINATE;
        case(CRC_ERROR) :
            break;
        default :
            DEBUG_PRINT("server: received bad packet in SEND_EOF\n");
            break;
    }

    return SEND_EOF;
}

// returns true if main server, else false
static bool handle_client(SParams* sp, UDPInfo* udp) {
    STATE state = SETUP;
    int client_sock = 0;
    Window* w = NULL;

    uint8_t pack[MAX_PACK]; // allows passing packets between states
    bool run = true;
   
    while(run) {
        switch(state) {
            case(SERVER_PROCESS) :
                DEBUG_PRINT("server: returning to handle new clients\n");
                return true;
            case(SETUP) :
                DEBUG_PRINT("server: setting up new client\n");
                state = FSM_setup_client(&client_sock, sp);
                break;
            case(SETUP_ACK) :
                DEBUG_PRINT("server: acknowledging new client\n");
                state = FSM_setup_ack(client_sock, (void*)pack, udp);
                break;
            case(GET_PARAMS) :
                DEBUG_PRINT("server: parsing setup params\n");
                state = FSM_get_params(client_sock, (void*)pack, &w, udp, sp); 
                break;
            case(DATA_TX) :
                state = FSM_data_tx(client_sock, w, udp, sp);
                break;
            case(DATA_RECOV) :
                DEBUG_PRINT("server: responding to srej\n");
                state = FSM_data_recov(client_sock, w, udp, sp);
                break;
            case(SEND_EOF) :
                DEBUG_PRINT("server: ending file transfer\n");
                state = FSM_send_eof(client_sock, udp, sp);
                break;
            case(TERMINATE) :
                DEBUG_PRINT("server: sayonara\n");
                if (sp->read_fd != NULL) fclose(sp->read_fd);
                if (w != NULL) del_window(w);
                return false;
                break;
            default:
                DEBUG_PRINT("server FSM: bad state\n");
                state = TERMINATE;
                break;
        }
    }

    return false;
}

// returns true for server, false for child
static bool handle_new_client(int server_sock, SParams* sp) {
    int flag, psize;
    UDPInfo udp = {{0}};
    uint8_t pack[MAX_PACK];

    flag = recv_rc_pack((void*)pack, MAX_PACK, &psize, server_sock, &udp);
    
    DEBUG_PRINT("CONNECTED IP=%s\n", ipAddressToString(&udp.addr));
    DEBUG_PRINT("iplen = %d\n", udp.addr_len);
  
    switch(flag) {
        case(FLAG_SETUP):
            DEBUG_PRINT("main_server: valid connection request\n");
            return handle_client(sp, &udp);
        case(CRC_ERROR):
            DEBUG_PRINT("main_server: CRC_ERROR\n");
            break;
        default:
            DEBUG_PRINT("main_server: bad packet received\n");
            break;
    }

    return true;
}

// Note, with current implementation, zombies will pile up if tons of connection requests
static void start_server(SParams* sp) {
    int server_sock, status = 0;

    DEBUG_PRINT("Starting Server\n");

 	server_sock = udpServerSetup(sp->port);

    while(1) {
        if (select_call(server_sock, NEW_CLIENT_TIMEOUT_S, 0, USE_TIMEOUT)) {
            while(waitpid(-1, &status, WNOHANG) > 0); // check on children
        } else { // new client request
            if (!handle_new_client(server_sock, sp)) break;
        }
    }
}

int main(int argc, char* argv[]) {
    SParams sp = {0};

    #ifdef DEBUG
        setvbuf(stdout, NULL, _IONBF, 0);
    #endif
    
    init_sp(&sp);
    get_args(argc, argv, &sp);

    sendErr_init(sp.err, DROP_ON, FLIP_ON, 
        #ifdef DEBUG 
            DEBUG_ON, 
        #else
            DEBUG_OFF, 
        #endif
            RSEED_ON);
   
    start_server(&sp);

    exit(EXIT_SUCCESS);
}
