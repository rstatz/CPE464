#ifndef CLIENT_TABLE_H
#define CLIENT_TABLE_H

#include <stdint.h>

#include "linkedlist.h"

typedef struct Client_Info {
    int sock;
    char handle[101];
} Client_Info;

typedef struct Client_Table {
    int num_clients;
    LLNode* entries;
} Client_Table;

Client_Table* new_ctable();

void new_client(Client_Table*, int, char*);

int rm_client(Client_Table*, int);

int get_socket(Client_Table* table, char* handle);

// Functions for parsing through table entries
void* get_stream(Client_Table*);
Client_Info* get_next_entry(void** stream); // Returns NULL when reached end

#endif
