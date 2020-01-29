#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "linkedlist.h"
#include "client_table.h"

int match_socket(void* sock, void* ci) {
    int sock_ci = ((Client_Info*)ci)->sock;

    return *((int*)sock) == sock_ci;
}

int match_handle(void* handle, void* ci) {
    char* handle_ci = ((Client_Info*)ci)->handle;

    return strcmp(handle, handle_ci) == 0;
}

void delete_ci(void* ci) {
    free(ci);
}

Client_Table* new_ctable() {
    Client_Table* table = malloc(sizeof(Client_Table));

    table->num_clients = 0;
    table->entries = NULL;

    return table;
}

void new_client(Client_Table* table, int sock, char* handle) {
    Client_Info* ci = malloc(sizeof(Client_Info));

    ci->sock = sock;
    strcpy(ci->handle, handle);

    table->entries = ll_append(table->entries, ci);
}

int rm_client(Client_Table* table, int sock) {
    table->entries = ll_sremove(table->entries, (void*)&sock, (*match_socket), (*delete_ci));
}

int get_socket(Client_Table* table, char* handle) {
    Client_Info* ci = (Client_Info*)ll_search(table->entries, (void*)handle, (*match_handle));

    if (ci == NULL)
        return -1;

    return ci->sock;
}

void* get_stream(Client_Table* table) {
    return ll_get_stream(table->entries);
}

Client_Info* get_next_entry(void** stream) {
    return (Client_Info*)ll_get_next_entry(stream);
}
