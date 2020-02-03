#ifndef CHAT_CLIENT_CLI_H
#define CHAT_CLIENT_CLI_H

#define MAX_CMD_LENGTH_BYTES 1400

#define TEXT_BLOCK_SIZE 200
#define MAX_NUM_TEXT_BLOCKS (MAX_CMD_LENGTH_BYTES/TEXT_BLOCK_SIZE)

#define FILL_BUFFER true
#define STOP_AT_SPACE false

void cli_parse_msg(char* cmd, int sock, char* my_handle);
void cli_parse_broadcast(char* cmd, int sock, char* my_handle);

#endif
