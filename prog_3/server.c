#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "debug.h"
#include "networks.h"

#ifdef DEBUG
#include "test_rcopy.h"
#endif

enum STATE {SETUP=0, FILENAME, DATA_RX, TERMINATE};

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

static void handle_clients(int sock, float err) {

}

int main(int argc, char* argv[]) {
    float err;
    int port, sock;

    get_args(argc, argv, &err, &port);

    test_window(); 

//	sock = udpServerSetup(port);

//    handle_clients(sock, err);

    exit(EXIT_SUCCESS);
}
