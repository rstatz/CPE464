#include <stdlib.h>
#include <stdbool.h>
#include <pcap/pcap.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ether.h>

#include "packets.h"
#include "checksum.h"

#define TOS_MASK 0x3F
#define GET_TOS(A) (((A << 2) + ((A >> 6) & 0x3)) & TOS_MASK)

#define NO_MORE_PACKETS -2

#define IPV4_TYPE 0x0800
#define ARP_TYPE 0x0806
#define ICMP_TYPE 1
#define TCP_TYPE 6
#define UDP_TYPE 17

#define ARP_REQUEST 1
#define ARP_REPLY 2

#define ICMP_REQUEST 8
#define ICMP_REPLY 0

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

    type = ntohs(ehead->ether_type);

    printf("\n\tEthernet Header\n");

    dest_mac_bin = ehead->dest_mac;
    dest_mac = ether_ntoa((const struct ether_addr*)&dest_mac_bin);
    printf("\t\tDest MAC: %s\n", dest_mac);

    src_mac_bin = ehead->src_mac;
    src_mac = ether_ntoa((const struct ether_addr*)&src_mac_bin);
    printf("\t\tSource MAC: %s\n", src_mac);
    
    switch(type) {
        case(IPV4_TYPE) :
            printf("\t\tType: IP\n");
            break;
        case(ARP_TYPE) :
            printf("\t\tType: ARP\n");
            break;
        default:
            printf("\t\tType: Unknpwn\n");
    }

    *payload = ehead + 1;

    return type;
}

void tcp_setup(IPV4_header* ipv4h, TCP_header* tcph) {
    uint32_t src_ip = ipv4h->src_ip;
    uint32_t dest_ip = ipv4h->dest_ip;
    uint8_t protocol = ipv4h->protocol;
    uint16_t len;
    p_TCP_header* ptcph = ((p_TCP_header*)tcph) - 1;
    
    // len = ip pkt length - ip header length
    len = ntohs(ipv4h->len) - (ipv4h->ihl * 4); 

    ptcph->src_ip = src_ip;
    ptcph->dest_ip = dest_ip;
    ptcph->protocol = protocol;
    ptcph->res = 0;
    ptcph->len = htons(len);
}

uint16_t parse_ipv4_packet(void* pkt, void** payload) {
    IPV4_header* ipv4h = (IPV4_header*)pkt;
    uint16_t header_len, c_checksum;
    uint8_t protocol;
    char *src_ip, *dest_ip;
    bool tcp = false, known = true;

    header_len = ipv4h->ihl * 4;

    //tos = GET_TOS(ipv4h->tos_ecn);
    protocol = ipv4h->protocol;

    printf("\n\tIP Header\n");
    printf("\t\tHeader Len: %d (bytes)\n", header_len);
    printf("\t\tTOS: 0x%x\n", ipv4h->tos_ecn);
    printf("\t\tTTL: %d\n", ipv4h->ttl);
    printf("\t\tIP PDU Len: %d (bytes)\n", ntohs(ipv4h->len));
    
    switch(protocol) {
        case(TCP_TYPE) :
            printf("\t\tProtocol: TCP\n");
            tcp = true;
            break;
        case(UDP_TYPE) :
            printf("\t\tProtocol: UDP\n");
            break;
        case(ICMP_TYPE) :
            printf("\t\tProtocol: ICMP\n");
            break;
        default:
            known = false;
            printf("\t\tProtocol: Unknown\n");
    }

    c_checksum = in_cksum((unsigned short*)ipv4h, ipv4h->ihl * 4); 
    printf("\t\tChecksum: %s (0x%x)\n", c_checksum ? "Incorrect" : "Correct", ipv4h->checksum);

    src_ip = inet_ntoa(((union ip_in_addr)(uint64_t)ipv4h->src_ip).ip);
    printf("\t\tSender IP: %s\n", src_ip);

    dest_ip = inet_ntoa(((union ip_in_addr)((uint64_t)ipv4h->dest_ip)).ip);
    printf("\t\tDest IP: %s\n", dest_ip);
    
    *payload = (void*)(((char*)ipv4h) + header_len);

    // sets up pseudo header for TCP
    if (tcp)
        tcp_setup(ipv4h, (TCP_header*)*payload);

    return known ? protocol : 0;
}

uint16_t parse_arp_packet(void* pkt, void** payload) {
    ARP_header* arph = (ARP_header*)pkt;
    uint16_t opcode;
    uint64_t src_mac_bin, dest_mac_bin;
    char *src_ip, *src_mac, *dest_ip, *dest_mac;

    opcode = ntohs(arph->oper);

    printf("\n\tARP header\n");
    
    switch(opcode) {
        case(ARP_REQUEST) :
            printf("\t\tOpcode: Request\n");
            break;
        case(ARP_REPLY) :
            printf("\t\tOpcode: Reply\n");
            break;
        default:
            printf("\t\tOpcode: Unknown\n");
            break;
    }

    src_mac_bin = arph->sha;
    src_mac = ether_ntoa((const struct ether_addr*)&src_mac_bin);
    printf("\t\tSender MAC: %s\n", src_mac);

    src_ip = inet_ntoa(((union ip_in_addr)((uint64_t)arph->spa)).ip);
    printf("\t\tSender IP: %s\n", src_ip);

    dest_mac_bin = arph->tha;
    dest_mac = ether_ntoa((const struct ether_addr*)&dest_mac_bin);
    printf("\t\tTarget MAC: %s\n", dest_mac);

    dest_ip = inet_ntoa(((union ip_in_addr)((uint64_t)arph->tpa)).ip);
    printf("\t\tTarget IP: %s\n", dest_ip);
    
    *payload = arph + 1;
    
    return 0;
}

uint16_t parse_udp_packet(void* pkt, void** payload) {
    UDP_header* udph = (UDP_header*) pkt;
    uint16_t src_port, dest_port;

    src_port = ntohs(udph->src_port);
    dest_port = ntohs(udph->dest_port);

    *payload = pkt + 1; 

    printf("\n\tUDP Header\n");
    printf("\t\tSource Port: %d\n", src_port);
    printf("\t\tDest Port: %d\n", dest_port);

    return udph->length;
}

// len is the length of segment header, and data seg
uint16_t calc_tcp_cksum(TCP_header* tcph) {
    p_TCP_header* ptcph = ((p_TCP_header*)tcph) - 1;
    
    return in_cksum((unsigned short*)ptcph, htons(ptcph->len) + sizeof(p_TCP_header));
}

uint16_t parse_tcp_packet(void* pkt, void** payload) {
    TCP_header* tcph = (TCP_header*)pkt;
    uint16_t c_checksum;
    
    printf("\n\tTCP Header\n");
    printf("\t\tSource Port: %d\n", ntohs(tcph->src_port));
    printf("\t\tDest Port: %d\n", ntohs(tcph->dest_port));
    
    printf("\t\tSequence: %u\n", ntohl(tcph->seq_number));
    printf("\t\tACK Number: %u\n", ntohl(tcph->ack_number));

    printf("\t\tACK Flag: %s\n", (tcph->ack ? "Yes" : "No"));
    printf("\t\tSYN Flag: %s\n", (tcph->syn ? "Yes" : "No"));
    printf("\t\tRST Flag: %s\n", (tcph->rst ? "Yes" : "No"));
    printf("\t\tFIN Flag: %s\n", (tcph->fin ? "Yes" : "No"));

    printf("\t\tWindow Size: %d\n", ntohs(tcph->window_size));

    c_checksum = calc_tcp_cksum(tcph);
    printf("\t\tChecksum: %s (0x%x)\n", c_checksum ? "Incorrect" : "Correct", ntohs(tcph->checksum));

    *payload = ((uint32_t*)tcph) + tcph->offset;

    return 0;
}

uint16_t parse_icmp_packet(void* pkt, void** payload) {
    ICMP_header* icmph = (ICMP_header*)pkt;
    
    printf("\n\tICMP Header\n");

    switch(icmph->type) {
        case(ICMP_REQUEST) :
            printf("\t\tType: Request\n");
            break;
        case(ICMP_REPLY) :
            printf("\t\tType: Reply\n");
            break;
        default:
            printf("\t\tType: %d\n", icmph->type);
    }

    *payload = icmph + 1;

    return 0;
}

uint16_t parse_l2_pkt(void* pkt, uint16_t type, void** payload) {
    switch(type) {
        case(IPV4_TYPE) :
            type = parse_ipv4_packet(pkt, payload);
            break;
        case(ARP_TYPE) :
            type = parse_arp_packet(pkt, payload);
            break;
        default:
            printf("Unrecognized L2 Packet type\n");
   }

   return type;
}

uint16_t parse_l3_pkt(void* pkt, uint16_t type, void** payload) {
    uint16_t len;

    switch(type) {
        case(TCP_TYPE) :
            len = parse_tcp_packet(pkt, payload); // Doesn't provide length
            break;
        case(UDP_TYPE) :
            len = parse_udp_packet(pkt, payload);
            break;
        case(ICMP_TYPE) :
            len = parse_icmp_packet(pkt, payload);
            break;
        default:
            printf("Unrecognized L3 Packet type\n");
    }
    
    return len;
}

void parse_packet(struct pcap_pkthdr* header, const u_char* data, int n) {
    void* eth_pkt = (void*)data;
    void* l2_pkt;
    void* l3_pkt;
    void* payload;

    uint16_t type;

    type = parse_ethernet_packet(eth_pkt, &l2_pkt);

    if (type)
        type = parse_l2_pkt(l2_pkt, type, &l3_pkt);

    if (type) 
        type = parse_l3_pkt(l3_pkt, type, &payload);
}

void parse_packets(pcap_t* pcap) {
    struct pcap_pkthdr* header;
    const u_char* data;

    int n = 1;

    while (next_packet(pcap, &header, &data)) {
        printf("Packet number: %d Frame Len: %d\n", n, header->len);
        parse_packet(header, data, n);
        
        n++;
        printf("\n");
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
