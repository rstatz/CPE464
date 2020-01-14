#include <stdlib.h>
#include <stdbool.h>
#include <pcap/pcap.h>
#include <netinet/ether.h>

#include "packets.h"

#define NO_MORE_PACKETS -2

void err_usage() {
    printf("usage : trace input_file");
    exit(EXIT_FAILURE);
}

pcap_t* open_pcap(char* pcap_f) {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t* pcap;

    if ((pcap = pcap_open_offline(pcap_f, errbuf)) == NULL) {
        printf(errbuf);
        exit(EXIT_FAILURE);
    }

    return pcap;
}

int next_packet(pcap_t* pcap, struct pcap_pkthdr** header, const u_char** data) {
    int err;

    if (!(err = pcap_next_ex(pcap, header, data))) {
        switch(err) {
            case -1: pcap_perror(pcap, "error:");
                     break;

            case NO_MORE_PACKETS: break;

            default: printf("error: Unknown pcap error occurred");
                     break;
        }

        return 0;
    }
   
    return 1;
}

void parse_ethernet_packet() {

}

void parse_packet(struct pcap_pkthdr* header, const u_char* data, int n) {
    void* pkt = (void*)data; // TODO: check endianness
   
    Ethernet_header* ehead = (Ethernet_header*)data; 
    uint64_t dest_mac = ehead->dest_mac;

    printf("preamble: %lu", ehead->preamble);
    printf("Packet Number: %d\n", n);
    printf("%x:%x:%x:%x:%x:%x\n", (unsigned int)(dest_mac >> 40) & 0xFF,
                                  (unsigned int)(dest_mac >> 32) & 0xFF,
                                  (unsigned int)(dest_mac >> 24) & 0xFF,
                                  (unsigned int)(dest_mac >> 16) & 0xFF,
                                  (unsigned int)(dest_mac >> 8) & 0xFF,
                                  (unsigned int)dest_mac  & 0xFF);
    
}

void parse_packets(pcap_t* pcap) {
    struct pcap_pkthdr* header;
    const u_char* data;

    int n = 0;

    while (next_packet(pcap, &header, &data)) {
        parse_packet(header, data, n);
        
        n++;
    }
}

int main(int argc, char* argv[]) {
    pcap_t* pcap;

    if (argc != 2)
        err_usage();

    // Open pcap file
    pcap = open_pcap(argv[1]);

    // Parse pcap packets
    parse_packets(pcap);

    // Close pcap
    pcap_close(pcap);
    
    exit(EXIT_SUCCESS);
}
