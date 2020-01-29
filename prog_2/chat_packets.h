#ifndef CHAT_PACKETS_H
#define CHAT_PACKETS_H

#include <stdint.h>

typedef struct Chat_header {
    uint16_t PDU_length;
    uint8_t flag;
} __attribute__((packed)) Chat_header;

typedef struct Handle_Pack {
    uint8_t handle_length;
    
    char handle;
} __attribute__((packed)) Handle_Pack;

// Flag = 1 : Client connection
// |header|source handle|

// Flag = 2 : Server ACK for Client Connection
// |header|

// Flag = 3 : Server ERR for Client Connection
// |header|

// Flag = 4 : Broadcast
// |header|source handle|

// Flag = 5 : Message
// |header|source handle|dest handles...|

// Flag = 7 : Handle name error
// |header|incorrect dest handle|

// Flag = 8 : Client Exit Request
// |header|

// Flag = 9 : Client Exit ACK
// |header|

// Flag = 10 : Handle List Request
// |header|

// Flag = 11 : Number of Clients
// |header|number|

// Flag = 12 : Client Handle Entry
// |header|handle|

// Flag = 13 : Handle List End
// |header|

#endif
