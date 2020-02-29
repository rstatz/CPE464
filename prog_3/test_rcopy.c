#include <stdio.h>
#include <string.h>

#include "test_rcopy.h"
#include "window.h"

#define TEST_WINDOW_SIZE 5

void test_get_packet(Window* w, uint32_t seq) {
    void* ret = get_packet(w, seq);

    if (ret == NULL)
        printf("getpack: error retrieving %d\n", seq);
    else
        printf("Packet %d: %s\n", seq, (char*)ret);      
}

void test_buf(Window* w, uint32_t seq, void* pack, int psize) {
    int ret = buf_packet(w, seq, pack, psize);

    if (ret == PACKET_EXISTS)
        printf("buf: Packet %d already exists\n", seq);
    else if (ret == -1)
        printf("buf: err\n");
    else
        printf("buf: buffered %d\n", seq);
}

void test_move_window(Window* w, int n) {
    if (move_window_n(w, n) == -1)
        printf("mvwndw: err\n");
    else 
        printf("mvwndw: moved window %d\n", n);
}

void test_window() {
    int i;
    Window* w;
    
    char* pack[9] = {"",
                     "apple",
                     "butter", 
                     "crabs", 
                     "derby", 
                     "epinephren",
                     "fergy",
                     "galoshes",
                     "hoboken"};

    w = get_window(TEST_WINDOW_SIZE);

    for (i = 1; i < 3; i++) {
        test_buf(w, i, (void*)pack[i], strlen(pack[i]));
    }

    test_buf(w, 2, (void*)pack[2], strlen(pack[2]));
  
    test_move_window(w, 1);

    test_get_packet(w, 1);
    test_get_packet(w, 2);

    test_move_window(w, 1);

    test_get_packet(w, 2);

    test_move_window(w, 1);

    for (i = 3; i < 9; i++) {
        if (i != 5) {
            test_buf(w, i, (void*)pack[i], strlen(pack[i]));     
        }
    }

    for (i = 3; i < 9; i++) {
        test_get_packet(w, i);
    }
}
