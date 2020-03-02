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

STATE FSM_setup_client(int* sock, UDPInfo* udp) {
    // TODO fork

    // close server socket?

    // get new client socket
    *sock = get_UDP_socket();

    return SETUP_ACK;
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
    return FILENAME;
}

static void handle_client(float err, UDPInfo* udp) {
    int client_sock;
    uint8_t pack[MAX_PACK]; // allows passing packets between states
    STATE state = SETUP_ACK;
    bool run = true;

    while(run) {
        switch(state) {
            case(SETUP) :
                state = FSM_setup_client(&client_sock, udp);
                break;
            case(SETUP_ACK) :
                state = FSM_setup_ack(client_sock, err, (void*)pack, udp);
                state = TERMINATE; // TODO temp for testing
                break;
            case(FILENAME) :
                break;
            case(FILENAME_ACK) :
                break;
            case(DATA_TX) :
                break;
            case(DATA_RECOV) :
                break;
            case(SEND_EOF) :
                break;
            case(TERMINATE) :
                close(client_sock);
                // TODO other cleanup?
                run = false;
                break;
            default:
                DEBUG_PRINT("Server FSM: bad state\n");
                state = TERMINATE;
                break;
        }
    }
}

static void start_server(int server_sock, UDPInfo* udp) {
    fd_set rfds;
    int flag;
    
    uint8_t pack[MAX_PACK];
 
    FD_ZERO(&rfds);
    FD_SET(server_sock, &rfds);    

    while(1) {
        DEBUG_PRINT("SELECT\n");
        select(server_sock + 1, &rfds, NULL, NULL, NULL);
        DEBUG_PRINT("WOKE\n");
        
        flag = recv_rc_pack((void*)pack, MAX_PACK, server_sock, udp);
        
        switch(flag) {
            case(FLAG_SETUP):
                DEBUG_PRINT("start_server: valid connection request\n");
                return;
            case(CRC_ERROR):
                DEBUG_PRINT("start_server: CRC_ERROR\n");
                break;
            default:
                DEBUG_PRINT("start_server: bad packet received\n");
                break;
        }
    }
}

int main(int argc, char* argv[]) {
    float err;
    int port, server_sock;
    UDPInfo udp;

    get_args(argc, argv, &err, &port);

//    test_window(); 

	server_sock = udpServerSetup(port);

    start_server(server_sock, &udp);

    handle_client(err, &udp);

    exit(EXIT_SUCCESS);
}
