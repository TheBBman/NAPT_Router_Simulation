#include <iostream>
#include <string>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>
#pragma pack(1)
struct IP_header{
    uint8_t version :4;
    uint8_t header_length :4;
    uint8_t types_of_service;
    uint16_t total_length;
    uint32_t stuff;
    uint8_t TTL;
    uint8_t protocol;
    uint16_t ip_checksum;
    uint32_t source_ip;
    uint32_t dest_ip;
};
#pragma pack()
//main function to process a single IP packet
void processIPPacket(char* buffer, size_t length){
    //cast buffer to IP header to extract information
    const struct IP_header* ip = (const struct IP_header*)buffer;
    //extract relevant fields (in correct byte order)
    uint8_t version = ip->version;
    uint8_t header_length = ip->header_length;
    uint16_t total_length = ntohs(ip->total_length);
    uint8_t protocol = ip->protocol;
    uint16_t ip_checksum = ntohs(ip->ip_checksum);
    struct in_addr source_ip;
    struct in_addr dest_ip;
    source_ip.s_addr = ntohl(ip->source_ip);
    dest_ip.s_addr = ntohl(ip->dest_ip);

    //test if its right:
    printf("IP packet: version = %u, header_length = %u, total_length = %u, protocol = %u, ip_checksum = %u\n",
           version, header_length, total_length, protocol, ip_checksum);
    printf("source_ip = %s, dest_ip = %s\n",
           inet_ntoa(source_ip), inet_ntoa(dest_ip));
}