#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "chat_test.h"
#include "client_table.h"

void test_new_client(Client_Table* table, int socket, char* handle) {
    printf("Adding client \"%s\"\n", handle);
    new_client(table, socket, handle);
}

void test_rm_client(Client_Table* table, int socket) {
    printf("Removing client with socket # %d\n", socket);
    rm_client(table, socket);
}

int test_get_socket(Client_Table* table, char* handle) {
    int sock = get_socket(table, handle);

    if (sock == -1)
        printf("Handle \"%s\" not found\n", handle);
    else
        printf("Socket #: %d\n", sock);

    return sock;
}

void test_table() {
    char h1[100], h2[100], h3[100];
    int s1 = 1, s2 = 2, s3 = 3;
    Client_Table* table = new_ctable();

    strcpy(h1, "test1");
    strcpy(h2, "test2");
    strcpy(h3, "test3");

    test_new_client(table, s1, h1);

    test_get_socket(table, h1);
    test_get_socket(table, h2);

    test_rm_client(table, s1);

    test_new_client(table, s3, h3);
    test_new_client(table, s1, h1);
    test_new_client(table, s2, h2);

    test_get_socket(table, h1);
    test_get_socket(table, h2);
    test_get_socket(table, h3);

    test_rm_client(table, s2);
    test_rm_client(table, s3);

    test_get_socket(table, h3);

    test_rm_client(table, s1);
    test_rm_client(table, s1);

    free(table);
}
