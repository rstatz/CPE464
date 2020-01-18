#include <stdlib.h>
#include <stdbool.h>
#include <pcap/pcap.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ether.h>

#include "packets.h"

#define NO_MORE_PACKETS -2

#define IPV4_TYPE 0x0800
#define ARP_TYPE 0x0806
#define TCP_TYPE 6
#define UDP_TYPE 17

union ip_in_addr {
    uint64_t d_ip;
    struct in_addr ip;
};

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

    if ((err = pcap_next_ex(pcap, header, data)) != 1) {
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

uint16_t parse_ethernet_packet(void* pkt, void** payload) {
    Ethernet_header* ehead = (Ethernet_header*)pkt; 
    uint64_t dest_mac_bin, src_mac_bin;
    char *dest_mac, *src_mac;
    uint16_t type;

    dest_mac_bin = ehead->dest_mac;
    dest_mac = ether_ntoa((const struct ether_addr*)&dest_mac_bin);

    src_mac_bin = ehead->src_mac;
    src_mac = ether_ntoa((const struct ether_addr*)&src_mac_bin);

    type = ntohs(ehead->ether_type);

    printf("\tEthernet Header\n");
    printf("\t\tDest MAC: %s\n", dest_mac);
    printf("\t\tSource MAC: %s\n", src_mac);
    printf("\t\tType: 0x%04x\n", type);

    *payload = ehead + 1;

    return type;
}

uint16_t parse_ipv4_packet(void* pkt, void** payload) {
    IPV4_header* ipv4h = (IPV4_header*)pkt;
    uint8_t ttl, tos;
    uint16_t header_len, ip_pdu_len;
    char *src_ip, *dest_ip;

    header_len = ipv4h->ihl * 4;
    tos = ipv4h->tos;
    ttl = ipv4h->ttl;
    ip_pdu_len = ntohs(ipv4h->len);

    // for some reason the same?!
    printf("\tIP Header\n");
    printf("\t\tHeader Len: %d (bytes)\n", header_len);
    printf("\t\tTOS: 0x%x\n", tos);
    printf("\t\tTTL: %d\n", ttl);
    printf("\t\tIP PDU Len: %d\n", ip_pdu_len);
    printf("\t\tProtocol: %d\n", ipv4h->protocol);
 
    src_ip = inet_ntoa(((union ip_in_addr)(uint64_t)ipv4h->src_ip).ip);
    printf("\t\tSender IP: %s\n", src_ip);

    dest_ip = inet_ntoa(((union ip_in_addr)((uint64_t)ipv4h->dest_ip)).ip);
    printf("\t\tDest IP: %s\n", dest_ip);
    
    return ipv4h->protocol;
}

uint16_t 

uint16_t parse_l2_pkt(void* pkt, uint16_t type, void** payload) {
    switch(type) {
        case(IPV4_TYPE) :
            type = parse_ipv4_packet(pkt, payload);
            break;
        case(ARP_TYPE) :
            break;
        default:
            printf("Unrecognized L2 Packet type\n");
            break;
   }

   return type;
}

uint16_t parse_l3_pkt(void* pkt, uint16_t type, void** payload) {
    switch(type) {
        case(TCP_TYPE) :
            break;
        case(UDP_TYPE) :
            type = parse_udp_packet(pkt, payload);
            break;
        default:
            printf("Unrecognized L3 Packet type\n");
            break;
    }
    
    return type;
}

void parse_packet(struct pcap_pkthdr* header, const u_char* data, int n) {
    void* eth_pkt = (void*)data;
    void* l2_pkt;
    void* l3_pkt;

    uint16_t type;

    type = parse_ethernet_packet(eth_pkt, &l2_pkt);

    if (type)
        type = parse_l2_pkt(l2_pkt, type, &l3_pkt);
}

void parse_packets(pcap_t* pcap) {
    struct pcap_pkthdr* header;
    const u_char* data;

    int n = 1;

    while (next_packet(pcap, &header, &data)) {
        printf("Packet number: %d Frame Len: %d\n", n, header->len);
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
