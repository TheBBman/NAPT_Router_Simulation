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

using namespace std;
#pragma pack(1)
struct IP_header{
    uint8_t version:4;
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

//TCP protocol number: 6
#pragma pack(1)
struct TCP_header{
    uint16_t source_port;
    uint16_t dest_port;
    uint64_t stuffers;
    uint8_t header_length :4;
    uint8_t stuffies:4;
    uint8_t random;
    uint16_t window;
    uint16_t TCP_checksum;
};
#pragma pack()

//UDP protocol number:17
#pragma pack(1)
struct UDP_header{
    uint16_t source_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t UDP_checksum;
};
#pragma pack()


uint16_t compute_IP_checksum_value(const struct IP_header* ip){
    uint32_t sum = 0;
    struct IP_header ip_copy = *ip;
    
    // Set checksum field to zero for calculation
    ip_copy.ip_checksum = 0;
    uint16_t* ptr = (uint16_t*) &ip_copy;

    // Sum all 16-bit words in the header
    for (int i = 0; i < sizeof(struct IP_header) / 2; i++) {
        sum += *ptr++;
    }

    // Add carry bits to sum
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    // Return one's complement of sum
    //cout<<"the sum is"<<sum<<endl;
    //cout<<"total sum should be"<< ~(sum + ~sum)<<endl;
    return ~sum;
}
uint32_t calculate_checksum(uint16_t* ptr, size_t length) {
    uint32_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum += *ptr++;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return sum;
}

uint16_t compute_TCP_checksum(const struct IP_header* ip, const struct TCP_header* tcp, char* payload, size_t payload_length) {
    // Zero out the checksum field for calculation
    struct TCP_header tcp_copy = *tcp;
    //uint16_t old_checksum = tcp->TCP_checksum;
    tcp_copy.TCP_checksum = 0;

    // Calculate the checksum of the TCP header and payload
    uint32_t sum = calculate_checksum((uint16_t*) &tcp_copy, sizeof(struct TCP_header) / 2) +
                   calculate_checksum((uint16_t*)payload, payload_length / 2);
    
    
    // Add the checksum of the pseudo-header to the sum
    uint16_t pseudo_header[6];
    pseudo_header[0] = ip->source_ip >> 16;
    pseudo_header[1] = ip->source_ip & 0xFFFF;
    pseudo_header[2] = ip->dest_ip >> 16;
    pseudo_header[3] = ip->dest_ip & 0xFFFF;
    pseudo_header[4] = htons(ip->protocol);
    pseudo_header[5] = htons(sizeof(struct TCP_header) + payload_length);  // TCP length
    sum += calculate_checksum(pseudo_header, 6);

    // Add carry bits to sum
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    // Return one's complement of sum
    return ~sum;
}

uint16_t compute_UDP_checksum(const struct IP_header* ip, const struct UDP_header* udp, char* payload, size_t payload_length) {
    // Zero out the checksum field for calculation
    struct UDP_header udp_copy = *udp;
    //uint16_t old_checksum = tcp->TCP_checksum;
    udp_copy.UDP_checksum = 0;

    // Calculate the checksum of the TCP header and payload
    uint32_t sum1 =calculate_checksum((uint16_t*) &udp_copy, sizeof(struct UDP_header) / 2);
    uint32_t sum2 = calculate_checksum((uint16_t*)payload, payload_length / 2);
    uint32_t sum =  sum1 + sum2;
    
    //printf("initial sum is %u and %u\n", sum1, sum2);
    // Add the checksum of the pseudo-header to the sum
    uint16_t pseudo_header[6];
    pseudo_header[0] = ip->source_ip >> 16;
    pseudo_header[1] = ip->source_ip & 0xFFFF;
    pseudo_header[2] = ip->dest_ip >> 16;
    pseudo_header[3] = ip->dest_ip & 0xFFFF;
    pseudo_header[4] = htons(ip->protocol);
    pseudo_header[5] = htons(sizeof(struct UDP_header) + payload_length);  // TCP length
    sum += calculate_checksum(pseudo_header, 6);
    //printf("middle sum is %u\n", sum);
    // Add carry bits to sum
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    //printf("final sum is %u\n", sum);
    // Return one's complement of sum
    return ~sum;
}

//main function to process a single IP packet
int processIPPacket(char* buffer, size_t length, string& final_dest_ip_and_port, string& final_source_ip_and_port){
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
    char source_ip_str[INET_ADDRSTRLEN];
    char dest_ip_str[INET_ADDRSTRLEN];
    strncpy(source_ip_str, inet_ntoa(source_ip), INET_ADDRSTRLEN);
    strncpy(dest_ip_str, inet_ntoa(dest_ip), INET_ADDRSTRLEN);

    //test if its right:
    printf("IP packet: version = %u, header_length = %u, total_length = %u, protocol = %u, ip_checksum = %u, source_ip=%u, dest_ip =%u\n",
           version, header_length, total_length, protocol, ip_checksum, source_ip.s_addr, dest_ip.s_addr);
    printf("source_ip = %s, dest_ip = %s\n", source_ip_str, dest_ip_str);

    //check IP TTL, drop of TTL=0
    if (ip->TTL == 0){
        printf("packet dropped due to to TTL=0");
        return -1;
    }

    printf("performing checksums\n");
    //compute actual checksum value
    uint16_t actual_checksum = compute_IP_checksum_value(ip);
    //cout<<"actual checksum is " << actual_checksum<<endl;
    //cout <<"ip check sum is "<<ip_checksum<<endl;
    //perform IP header checksum
    //cout<<verify_IP<<endl;
    if (actual_checksum != ip_checksum){
        printf("packet dropped due to corrupted IP header\n");
        return -1;
    }

    uint16_t source_port;
    uint16_t dest_port;
    uint16_t transport_checksum;
    //check if TCP (protocol = 6) or UDP (protocol = 17):
    int header_len_bytes = header_length*4;
    uint16_t payload_length = total_length - header_len_bytes;
    printf("payload length is %u\n", payload_length);
    if (protocol == 6){
        //cast to TCP
        cout<<"its a tcp packet"<<endl;
        const struct TCP_header* tcp = (const struct TCP_header*)(buffer + header_len_bytes);
        //get source and dest port number
        source_port = ntohs(tcp->source_port);
        dest_port = ntohs(tcp->dest_port);
        transport_checksum = ntohs(tcp->TCP_checksum);

        uint16_t transport_header_length = sizeof(struct TCP_header);
        uint16_t actual_payload_length = payload_length - transport_header_length;
        //TCP checksum verification
        
        uint16_t transport_actual_checksum = compute_TCP_checksum(ip, tcp, buffer + header_len_bytes + transport_header_length, actual_payload_length);
        if (transport_checksum != transport_actual_checksum){
            cout<<"TCP checksum failed"<<endl;
            return -1;
        }
    }
    else if (protocol == 17){
        cout<<"its a udp packet"<<endl;
        const struct UDP_header* udp = (const struct UDP_header*)(buffer + header_len_bytes);
        //get source and dest port number
        source_port = ntohs(udp->source_port);
        dest_port = ntohs(udp->dest_port);
        transport_checksum = ntohs(udp->UDP_checksum);

        uint16_t transport_header_length = sizeof(struct UDP_header);
        uint16_t actual_payload_length = payload_length - transport_header_length;

        //printf("transport header length is %u\n", transport_header_length);
        uint16_t transport_actual_checksum = compute_UDP_checksum(ip, udp, buffer + header_len_bytes + transport_header_length, actual_payload_length);
        cout<<"transport checksum: "<<transport_actual_checksum<<endl;
        cout<<"checksum in payload: "<<transport_checksum<<endl;
        if (transport_checksum != transport_actual_checksum){
            cout<<"UDP checksum failed"<<endl;
            return -1;
        }
    }
    else{
        perror("unknown protocol");
    }

    //got the addresses and ports, now we modify the packets based on the three cases
    string source_ip_string (source_ip_str);
    string dest_ip_string (dest_ip_str);
    string source_port_string = to_string(source_port);
    string dest_port_string = to_string(dest_port);
    string LAN_PREFIX = "192.168.1";
    bool source_is_lan = source_ip_string.substr(0, LAN_PREFIX.length()) == LAN_PREFIX;
    bool dest_is_lan = dest_ip_string.substr(0, LAN_PREFIX.length()) == LAN_PREFIX;
    if (source_is_lan && dest_is_lan) {
        cout<<"this is local LAN forwarding, no modification needed"<<endl;
        final_dest_ip_and_port = dest_ip_string + " " +dest_port_string;
        final_source_ip_and_port = source_ip_string +" "+source_port_string;
        return 1;
    }


    final_dest_ip_and_port = "yomama";
}

void binaryStringToBytes(const char *binary, char *buffer, size_t len) {
    for (size_t i = 0; i < len; i += 8) {
        char temp[9] = {0};
        strncpy(temp, binary + i, 8);
        buffer[i / 8] = (char) strtol(temp, NULL, 2);
    }
}

