#ifndef PACKETS_H
#define PACKETS_H

#include <stdint.h>

typedef struct Ethernet_header {
    uint64_t preamble: 56;
    uint8_t delim: 8;
    
    uint64_t dest_mac: 48;
    uint64_t src_mac: 48;

    uint32_t tag_802_1Q: 32; // Optional, don't know about this
    
    uint16_t length: 16;
} __attribute__((packed)) Ethernet_header;

typedef struct ARP_header {
    uint16_t htype: 16;
    uint16_t ptype: 16;

    uint8_t hlen: 8;
    uint8_t plen: 8;

    uint16_t oper: 16;

    uint64_t sha: 48;
    
    uint32_t spa: 32;

    uint64_t tha: 48;

    uint32_t tpa: 32;
} __attribute__((packed)) ARP_header;

typedef struct ICMP_header {
    uint8_t type;
    uint8_t code;

    uint16_t checksum;

    uint32_t payload;
} __attribute__((packed)) ICMP_header;

typedef struct TCP_header {
    uint16_t src_port: 16;
    uint16_t dest_port: 16;

    uint32_t seq_number: 32;
    uint32_t ack: 32;
    
    uint8_t offset: 4;
    uint8_t reserved: 3;
    uint8_t ns: 1;
    uint8_t cwr: 1;
    uint8_t ece: 1;
    uint8_t urg: 1;
    uint8_t ack: 1;
    uint8_t psh: 1;
    uint8_t rst: 1;
    uint8_t syn: 1;
    uint8_t fin: 1;

    uint16_t window_size: 16;

    uint16_t checksum: 16;
    uint16_t urgent_ptr: 16;  
} __attribute__((packed)) TCP_header;

typedef struct UDP_header {
    uint16_t src_port;
    uint16_t dest_port;
    
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) UDP_header;

