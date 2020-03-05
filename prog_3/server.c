#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/select.h>

#include "debug.h"
#include "cpe464.h"
#include "networks.h"
#include "rcopy_packets.h"
#include "gethostbyname.h"

#define RESEND_TRIES 10

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
    FILE* read_fd;
} SParams;

static void usage() {
    fprintf(stderr, "Usage: server error-percent [port-number]\n");
    exit(EXIT_FAILURE);
}

static void get_args(int argc, char* argv[], float* err, int* port) {
    if (argc > 3 || argc < 2)
        usage();
    
    *err = strtof(argv[1], NULL);
    if (errno != 0)
        usage();
    if ((*err > 100) || (*err < 0)) {
        fprintf(stderr, "Invalid error-percent (0 to 100)\n");       
        usage();
    }

    *port = (argc == 3) ? atoi(argv[2]) : 0;
}

STATE FSM_setup_client(int* sock) {
    // TODO fork
    // server returns SERVER_PROCESS

    // close server socket?

    // get new client socket
    *sock = get_UDP_socket();

    return SETUP_ACK;
}

STATE FSM_setup_ack(int sock, void* pack, UDPInfo* udp) {    
    uint8_t setup_ack_pack[MAX_PACK];
    uint8_t tries = MAX_ATTEMPTS;
    bool done = false;

    build_setup_ack_pack((void*)setup_ack_pack); 

    while(!done) {
        done = true; // assume success

        if ((tries = select_resend_n(sock, TIMEOUT_VALUE_S, 0, USE_TIMEOUT, tries, udp)) == TIMEOUT_REACHED)
            return TERMINATE;

        switch(recv_rc_pack(pack, MAX_PACK, sock, udp)) {
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

static STATE FSM_get_params(int sock, void* setup_params_pack, UDPInfo* udp, SParams* sp) {
    uint8_t response_pack[MAX_PACK];
    uint8_t start_tx_pack[MAX_PACK];
    char* fname;

    uint8_t tries = MAX_ATTEMPTS;
    bool done = false;
 
    parse_setup_params(setup_params_pack, &sp->wsize, &sp->bsize, &fname);

    DEBUG_PRINT("RCOPY CONFIG VALUES:\n");
    DEBUG_PRINT("    FNAME=%s\n", fname);
    DEBUG_PRINT("    WSIZE=%d\n", sp->wsize);
    DEBUG_PRINT("    BSIZE=%d\n", sp->bsize);
    // TODO check these values server side?

    // check if file exists
    if ((sp->read_fd = fopen(fname, "r")) < 0) {
        DEBUG_PRINT("server: bad filename received, sending response\n");
        build_bad_fname_pack((void*)response_pack);
        send_last_rc_build(sock, udp);
        return TERMINATE;
    } 

    DEBUG_PRINT("server: good filename received, sending response\n");
    build_setup_params_ack_pack((void*)&response_pack);

    // send filename_ack
    while (!done) {
        done = true; // assume success
        
        if ((tries = select_resend_n(sock, TIMEOUT_VALUE_S, 0, USE_TIMEOUT, tries, udp)) == TIMEOUT_REACHED)
            return TERMINATE;

        switch(recv_rc_pack((void*)start_tx_pack, MAX_PACK, sock, udp)) {
            case(FLAG_RR) :
                DEBUG_PRINT("server: received RR"); // TODO check if 1?
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

// returns true if main server, else false
static bool handle_client(UDPInfo* udp) {
    STATE state = SETUP;
    int client_sock = 0;
    SParams sp = {0};

    uint8_t pack[MAX_PACK]; // allows passing packets between states
    bool run = true;

    while(run) {
        switch(state) {
            case(SERVER_PROCESS) :
                DEBUG_PRINT("server: returning to handle new clients\n");
                return true;
            case(SETUP) :
                DEBUG_PRINT("server: setting up new client\n");
                state = FSM_setup_client(&client_sock);
                break;
            case(SETUP_ACK) :
                DEBUG_PRINT("server: acknowledging new client\n");
                state = FSM_setup_ack(client_sock, (void*)pack, udp);
                break;
            case(GET_PARAMS) :
                DEBUG_PRINT("server: parsing setup params\n");
                FSM_get_params(client_sock, pack, udp, &sp);
                state = TERMINATE; // temp for testing
                break;
            case(DATA_TX) :
                DEBUG_PRINT("server: transmitting file data\n");
                break;
            case(DATA_RECOV) :
                break;
            case(SEND_EOF) :
                DEBUG_PRINT("server: ending file transfer\n");
                break;
            case(TERMINATE) :
                DEBUG_PRINT("server: sayonara\n");
                fclose(sp.read_fd);
                close(client_sock);
                // TODO other cleanup? remove run = false
                run = false;
                //exit(EXIT_SUCCESS);
                break;
            default:
                DEBUG_PRINT("Server FSM: bad state\n");
                state = TERMINATE;
                break;
        }
    }

    return false;
}

static void start_server(int port) {
    int flag, server_sock;
    uint8_t pack[MAX_PACK];
    UDPInfo udp = {{0}};
 
    DEBUG_PRINT("Starting Server\n");

 	server_sock = udpServerSetup(port);

    while(1) {
        select_call(server_sock, 0, 0, !USE_TIMEOUT);
        
        flag = recv_rc_pack((void*)pack, MAX_PACK, server_sock, &udp);
    
        DEBUG_PRINT("CONNECTED IP=%s\n", ipAddressToString(&udp.addr));
        DEBUG_PRINT("iplen = %d\n", udp.addr_len);
      
        switch(flag) {
            case(FLAG_SETUP):
                DEBUG_PRINT("main_server: valid connection request\n");
                if (handle_client(&udp))
                    break;
                return;
            case(CRC_ERROR):
                DEBUG_PRINT("main_server: CRC_ERROR\n");
                break;
            default:
                DEBUG_PRINT("main_server: bad packet received\n");
                break;
        }
    }
}

int main(int argc, char* argv[]) {
    float err = 0;
    int port;

    #ifdef DEBUG
        setvbuf(stdout, NULL, _IONBF, 0);
    #endif

    get_args(argc, argv, &err, &port);

    sendErr_init(err, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON); // TODO debug off
    
    start_server(port);

    exit(EXIT_SUCCESS);
}
