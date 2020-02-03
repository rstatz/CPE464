#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "debug.h"
#include "client_table.h"
#include "chat_packets.h"
#include "chat_client_cli.h"

// returns -1 on error
// otherwise return length of read plus the null terminator
int cli_get_next_string(char* cmd, int bufsize, char* buf, bool fillbuf) {
    char* cmd_start = cmd;
    int i = 0;
    
    while (isspace(*cmd) && !fillbuf) cmd++;

    if (*cmd == '\0')
        return -1;
    
    do {
        if (i == bufsize) {
            buf[bufsize - 1] = '\0';
            return bufsize;
        }
        if (isspace(cmd[i]) && !fillbuf)
            break;

        buf[i] = cmd[i];
        i++;
    } while(cmd[i] != '\0');

    buf[i] = '\0';
    return i + cmd - cmd_start;
}

// returns amount read
int cli_parse_msg_numh(char* cmd, uint8_t* numh) {
    char numh_s[20];
    int err, read = 0;

    // get number of dest handles
    read += (err = cli_get_next_string(cmd, 20, numh_s, STOP_AT_SPACE));
   
    if (err == -1) {
        printf("Invalid command format\n");
        return -1;
    }
    
    // check for digit
    if (isdigit(numh_s[0]))
        *numh = atoi(numh_s);
    else {
        *numh = 1;
        return 0;
    }

    if ((*numh <= 0) || (*numh > MAX_DEST_HANDLES)) {
        printf("Invalid number of destination handles, must be between 1 and 9\n");
        return -1;
    }

    return read;
}

// parse cmd for dest handles, returns number of characters read
int cli_parse_dest_handles(char* cmd, uint8_t* numh, char* handle_buf, char** hlist) {
    char trash[MAX_CMD_LENGTH_BYTES];
    char* cmd_start = cmd; 
    char* next_handle;
    int err, i;
    uint8_t numh_actual;

    for (i = 0, numh_actual = 0; i < *numh; i++, numh_actual++) {
        next_handle = handle_buf + (numh_actual * MAX_HANDLE_LENGTH_BYTES); 

        cmd += (err = cli_get_next_string(cmd, MAX_HANDLE_LENGTH_BYTES, next_handle, STOP_AT_SPACE));

        if (err == -1) {
            printf("Invalid command format\n");
            return -1;
        }
        if (err == (MAX_HANDLE_LENGTH_BYTES)) {
            printf("Invalid handle, handle longer than 100 characters: %s...\n", next_handle);   

            numh_actual--;
            cmd += 2; //reverses error
            cmd += cli_get_next_string(cmd, MAX_CMD_LENGTH_BYTES, trash, STOP_AT_SPACE); // clears out bad handle
        }
        else {
            DEBUG_PRINT("Dest handle %s\n", next_handle);

            hlist[numh_actual] = next_handle;
        }
    }

    *numh = numh_actual;

    return cmd - cmd_start;
}

uint8_t cli_parse_msg_text(char* cmd, char* text_buf, char** tlist) {
    char* cmdptr;
    char* text = text_buf;
    uint8_t numt = 0;

    do {
        text = text_buf + ((TEXT_BLOCK_SIZE) * numt);
        cmdptr = cmd + ((TEXT_BLOCK_SIZE - 1) * numt);

        tlist[numt++] = text;
    } while (cli_get_next_string(cmdptr, TEXT_BLOCK_SIZE, text, FILL_BUFFER) == TEXT_BLOCK_SIZE);

    return numt;
}

void cli_parse_msg(char* cmd, int sock, char* my_handle) {
    char handles[MAX_HANDLE_LENGTH_BYTES * MAX_DEST_HANDLES];
    char* hlist[MAX_DEST_HANDLES];
    char text_buf[MAX_CMD_LENGTH_BYTES];
    char* tlist[MAX_NUM_TEXT_BLOCKS];
    char* text;
    uint8_t numh, numt;
    int err, i;

    cmd += (err = cli_parse_msg_numh(cmd, &numh));
    
    if (err == -1)
        return;

    cmd += (err = cli_parse_dest_handles(cmd, &numh, handles, hlist));

    if (err == -1)
        return;
 
    numt = cli_parse_msg_text(cmd + 1, text_buf, tlist);

    // send message for every text block
    for (i = 0; i < numt; i++) {
        text = text_buf + (TEXT_BLOCK_SIZE * i);

        send_msg(sock, my_handle, numh, hlist, text);
    }
}

void cli_parse_broadcast(char* cmd, int sock, char* my_handle) {
    char text_buf[MAX_CMD_LENGTH_BYTES];
    char* tlist[MAX_NUM_TEXT_BLOCKS];
    char* text;
    int i, numt;

    numt = cli_parse_msg_text(cmd, text_buf, tlist);

    for (i = 0; i < numt; i++) {
        text = text_buf + (TEXT_BLOCK_SIZE * i);

        send_broadcast(sock, my_handle, text);
    }
} 
