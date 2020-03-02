#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/select.h>

#include "debug.h"
#include "networks.h"
#include "rcopy_packets.h"

#define MAX_RNAME 1000

typedef enum STATE {TERMINATE = -1,
                    SETUP = 0,
                    FILENAME,
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

static STATE FSM_setup(client_args* ca, UDPInfo* udp, int* sock) {
    // setup client socket
    *sock = setupUdpClientToServer(&udp->addr, ca->rname, ca->port);

    return FILENAME;
}

static void start_rcopy(client_args* ca) {
    STATE state = SETUP;
    UDPInfo udp;
    int client_sock; 
    bool run = true;
    
    while(run) {
        switch(state) {
            case(SETUP) :
                state = FSM_setup(ca, &udp, &client_sock);
                break;
            case(FILENAME) :
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

    get_args(argc, argv, &ca);

    start_rcopy(&ca);

    exit(EXIT_SUCCESS);
}
