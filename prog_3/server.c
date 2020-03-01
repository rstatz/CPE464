#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/select.h>

#include "debug.h"
#include "networks.h"
#include "rcopy_packets.h"

#define RESEND_TRIES 10

#ifdef DEBUG
#include "test_rcopy.h"
#endif

typedef enum STATE {TERMINATE=-1,
                    SETUP = 0,
                    SETUP_ACK,
                    FILENAME, 
                    FILENAME_ACK, 
                    DATA_TX, 
                    DATA_RECOV, // still needed with current window implementation?
                    SEND_EOF} STATE;

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

static void start_server(int server_sock) {
    fd_set rfds;
    int active_fds;
 
    FD_ZERO(&rfds);
    FD_SET(server_sock, &rfds);    

//    while(1) {
        DEBUG_PRINT("SELECT\n");
        active_fds = select(server_sock + 1, &rfds, NULL, NULL, NULL);
        DEBUG_PRINT("WOKE\n");
        
        // check CRC and do not process if bad

            // fork and handle new client
            // break on accepting new client and execute FSM
//    }
}

STATE FSM_setup_client(int sock, UDPInfo* udp) {
    // receive setup packet
    // if valid, state = setup_ack
    // else terminate
}

STATE FSM_setup_ack(int sock, float err, void* pack, UDPInfo* udp) {    
    build_setup_ack_pack((void*)pack); 

    // send packet
    // select
    // if timeout 
        // timeout == 10? terminate
        // else send again, increment timeout
    // else check flag
        // bad crc then resend
        // bad flag then ?
        // good then state = FILENAME
}

static void handle_client(int sock, float err) {
    uint8_t pack[MAX_PACK]; // allows passing packets between states
    UDPInfo udp;
    STATE state = SETUP_ACK;
    bool run = true;

    while(run) {
        switch(state) {
            case(SETUP) :
                state = FSM_setup_client(sock, &udp);
                break;
            case(SETUP_ACK) :
                state = FSM_setup_ack(sock, err, (void*)pack, &udp);
                break;
            case(FILENAME) :
                break;
            case(FILENAME_ACK) :
                break;
            case(DATA_TX) :
                break;
            case(DATA_RECOV) :
                break;
            case(TERMINATE) :
                // TODO cleanup
                run = false;
                break;
        }
    }
}

int main(int argc, char* argv[]) {
    float err;
    int port, sock;

    get_args(argc, argv, &err, &port);

//    test_window(); 

	sock = udpServerSetup(port);

    start_server(sock);

    handle_client(sock, err);

    exit(EXIT_SUCCESS);
}
