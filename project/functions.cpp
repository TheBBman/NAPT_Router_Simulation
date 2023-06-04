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
#include <sstream>

using namespace std;
#pragma pack(1)
struct IP_header{
    uint8_t header_length :4;
    uint8_t version:4;
    uint8_t types_of_service; //69
    uint16_t total_length; //7237
    uint32_t stuff; //7237 7301
    uint8_t TTL;
    uint8_t protocol; //11717
    uint16_t ip_checksum; //11717
    uint32_t source_ip; //54917, 80518
    uint32_t dest_ip; //123718, 174919
};
#pragma pack()

//TCP protocol number: 6
#pragma pack(1)
struct TCP_header{
    uint16_t source_port;
    uint16_t dest_port;
    uint64_t stuffers;
    uint8_t stuffies:4;
    uint8_t header_length :4;
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

string extractPrefix(const string& str) {
    string prefix;
    size_t dotCount = 0;

    for (std::size_t i = 0; i < str.length() && dotCount < 3; i++) {
        if (str[i] == '.') {
            prefix += str[i];
            dotCount++;
        } else {
            prefix += str[i];
        }
    }

    return prefix;
}

uint16_t compute_IP_checksum_value(const struct IP_header* ip){
    uint32_t sum = 0;
    struct IP_header ip_copy = *ip;
    
    // Set checksum field to zero for calculation
    //cout<<"checksum network "<<ip_copy.ip_checksum<<endl;
    ip_copy.ip_checksum = 0;
    uint16_t* ptr = (uint16_t*) &ip_copy;

    // Sum all 16-bit words in the header
    uint16_t  ip_header_len_bytes = ip_copy.header_length * 4;
    //cout<<"header len in 16 "<< ip_header_len_bytes/2<<endl;
    //cout<<"check "<<sizeof(struct IP_header) / 2<<endl;
    for (int i = 0; i < ip_header_len_bytes / 2; i++) {
        
        sum += *ptr++;
        cout<<sum<<endl;
    }

    // Add carry bits to sum
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    // Return one's complement of sum
    cout<<"the sum is"<<sum<<endl;
    cout<<"complement of sum is"<< ntohs(~(sum))<<endl;
    return ntohs(~sum);
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

    uint32_t host_source_ip = ip->source_ip;
    uint32_t host_dest_ip = ip->dest_ip;
    pseudo_header[0] = host_source_ip >> 16;
    pseudo_header[1] = host_source_ip & 0xFFFF;
    pseudo_header[2] = host_dest_ip >> 16;
    pseudo_header[3] = host_dest_ip & 0xFFFF;
    pseudo_header[4] = htons(ip->protocol);
    pseudo_header[5] = htons(sizeof(struct TCP_header) + payload_length);  // TCP length
    //cout<<"source pseudo" << pseudo_header[0]<<" "<<pseudo_header[1]<<endl;
    //cout<<"dest pseudo" << pseudo_header[2]<<" "<<pseudo_header[3]<<endl;
    sum += calculate_checksum(pseudo_header, 6);

    // Add carry bits to sum
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    // Return one's complement of sum
    return ntohs(~sum);
}

uint16_t compute_UDP_checksum(const struct IP_header* ip, const struct UDP_header* udp, char* payload, size_t payload_length) {
    // Zero out the checksum field for calculation
    struct UDP_header udp_copy = *udp;
    uint16_t old_checksum = udp_copy.UDP_checksum;
    cout<<"network udp "<<old_checksum<<endl;
    cout<<"host udp "<<ntohs(old_checksum)<<endl;
    udp_copy.UDP_checksum = 0;

    // Calculate the checksum of the TCP header and payload
    uint32_t sum1 =calculate_checksum((uint16_t*) &udp_copy, sizeof(struct UDP_header) / 2);
    uint32_t sum2 = calculate_checksum((uint16_t*)payload, payload_length / 2);
    uint32_t sum =  sum1 + sum2;
    
    //printf("initial sum is %u and %u\n", sum1, sum2);
    // Add the checksum of the pseudo-header to the sum
    //cout<<"total ip struct source ip"<<ip->source_ip<<endl;
    //cout<<"ntohl source ip"<< ntohl(ip->source_ip) <<endl;
    uint16_t pseudo_header[6];
    /*
        cout<<"starting"<<endl;
    uint16_t first = ip->source_ip >> 16;
    cout<< first <<endl;
    uint16_t unval = ip->source_ip & 0xFFFF;
    cout<< unval <<endl;
    cout<< htons(ip->source_ip >> 16) <<endl;
    uint16_t val = htons(ip->source_ip & 0xFFFF);
    cout<< val <<endl;
    */
    uint32_t host_source_ip = ip->source_ip;
    uint32_t host_dest_ip = ip->dest_ip;
    pseudo_header[0] = host_source_ip >> 16;
    pseudo_header[1] = host_source_ip & 0xFFFF;
    pseudo_header[2] = host_dest_ip >> 16;
    pseudo_header[3] = host_dest_ip & 0xFFFF;
    pseudo_header[4] = htons(ip->protocol);
    pseudo_header[5] = htons(sizeof(struct UDP_header) + payload_length);  // TCP length
    //cout<<"source pseudo " << pseudo_header[0]<<" "<<pseudo_header[1]<<endl;
    //cout<<"dest pseudo " << pseudo_header[2]<<" "<<pseudo_header[3]<<endl;
    sum += calculate_checksum(pseudo_header, 6);
    //printf("middle sum is %u\n", sum);
    // Add carry bits to sum
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    //printf("final sum is %u\n", sum);
    // Return one's complement of sum
    return ntohs(~sum);
}

pair<uint32_t, uint16_t> parseIPAndPort(const string& input) {
    istringstream stream(input);
    string ipAddress;
    string port;

    getline(stream, ipAddress, ' ');
    getline(stream, port, ' ');

    struct in_addr addr;
    inet_pton(AF_INET, ipAddress.c_str(), &(addr.s_addr));

    uint32_t ip = ntohl(addr.s_addr);  // Convert to host byte order
    //cout<<"host "<<ip<<"network "<<addr.s_addr<<endl;
    uint16_t portNum = stoi(port);

    return pair<uint32_t, uint16_t>(ip, portNum);
}

//main function to process a single IP packet
int processIPPacket(char* buffer, size_t length, string& final_source_ip_and_port, string& final_dest_ip_and_port, 
                    unordered_map<string, string>& int_NAT_table, unordered_map<string, string>& ext_NAT_table, string router_LanIp, string router_WanIp, int& cur_unused_port){
    //cast buffer to IP header to extract information
    const struct IP_header* ip = (const struct IP_header*)buffer;

    //extract relevant fields (in correct byte order)
    uint8_t version = ip->version;
    uint8_t header_length = ip->header_length;
    uint16_t total_length = ntohs(ip->total_length);
    uint8_t protocol = ip->protocol;
    uint16_t ip_checksum = ntohs(ip->ip_checksum);
    uint8_t TTL = ip->TTL;
    struct in_addr source_ip;
    struct in_addr dest_ip;
    source_ip.s_addr = ip->source_ip;
    dest_ip.s_addr = ip->dest_ip;
    char source_ip_str[INET_ADDRSTRLEN];
    char dest_ip_str[INET_ADDRSTRLEN];
    strncpy(source_ip_str, inet_ntoa(source_ip), INET_ADDRSTRLEN);
    strncpy(dest_ip_str, inet_ntoa(dest_ip), INET_ADDRSTRLEN);
    cout<<"in function"<<endl;
    //test if its right:
    fprintf(stdout,"IP packet: version = %u, header_length = %u, total_length = %u, protocol = %u, ip_checksum = %u, TTL= %u, source_ip=%u, dest_ip =%u\n",
           version, header_length, total_length, protocol, ip_checksum, TTL, ntohl(source_ip.s_addr), ntohl(dest_ip.s_addr));
    fprintf(stdout, "source_ip = %s, dest_ip = %s\n", source_ip_str, dest_ip_str);
    cout<<"end"<<endl;
    //check IP TTL, drop if TTL=0 or 1
    if (TTL <= 1){
        printf("packet dropped due to to TTL=0 or 1\n");
        return -1;
    }

    printf("performing checksums\n");
    //compute actual checksum value
    uint16_t actual_checksum = compute_IP_checksum_value(ip);
    cout<<"actual checksum is " << actual_checksum<<endl;
    cout <<"ip check sum is "<<ip_checksum<<endl;
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
        cout<<"transport header length: "<<transport_header_length<<endl;
        cout<<"actual payload_length: "<<actual_payload_length<<endl;
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
    //string source_ip_string = "10.05.05.01";
    //string dest_ip_string = "98.149.235.132";
    string source_port_string = to_string(source_port);
    string dest_port_string = to_string(dest_port);
    //string source_port_string = "1000";
    //string dest_port_string = "443";


    string LAN_PREFIX = extractPrefix(router_LanIp);
    cout<<"LAN prefix is " << LAN_PREFIX<<endl;
    cout<<"WAN address is " << router_WanIp<<endl;
    bool source_is_lan = source_ip_string.substr(0, LAN_PREFIX.length()) == LAN_PREFIX;
    bool dest_is_lan = dest_ip_string.substr(0, LAN_PREFIX.length()) == LAN_PREFIX;
    //local forwarding
    if (source_is_lan && dest_is_lan) {
        cout<<"this is local LAN forwarding, TTL needs to be decremented"<<endl;
        final_dest_ip_and_port = dest_ip_string + " " +dest_port_string;
        final_source_ip_and_port = source_ip_string +" "+source_port_string;

        //decrement TTL
        struct IP_header* modify_ip = (struct IP_header*)buffer;
        uint8_t new_TTL = TTL-1;
        modify_ip->TTL = new_TTL;
        uint16_t modified_ip_checksum = compute_IP_checksum_value(modify_ip);
        modify_ip->ip_checksum = htons(modified_ip_checksum);
        cout<<"new ip is "<<ntohl(modify_ip->source_ip)<<" new checksum is "<<ntohs(modify_ip->ip_checksum)<<endl;
        cout<<"new ip string is " <<final_source_ip_and_port<<endl;
        return 0;
    }
    string original_source_str = source_ip_string +" "+source_port_string;
    string original_dest_str = dest_ip_string + " " +dest_port_string;
    uint32_t modified_source_ip;
    uint16_t modified_source_port;
    uint32_t modified_dest_ip;
    uint16_t modified_dest_port;
    //local to external, use int_NAT_table to rewrite src
    if (source_is_lan && !(dest_is_lan)){
        cout<<"this is internal to external forwarding, modification needed for source "<<original_source_str<<endl;
        //check if already in table
        auto it = int_NAT_table.find(original_source_str); 
        //if the mapping doesn't exist, create one
        string modified_str;
        if (it == int_NAT_table.end()) {
            cout<<"entry does not exist, allocating new port"<<endl;
            string cur_unused_port_string = to_string(cur_unused_port);
            modified_str = router_WanIp +" "+cur_unused_port_string;
            int_NAT_table.insert({original_source_str, modified_str});
            //add for ext NAT table as well
            ext_NAT_table.insert({modified_str,original_source_str});
            cur_unused_port += 1;

            //check if successful
            auto check = int_NAT_table.find(original_source_str);
            auto check2 = ext_NAT_table.find(modified_str);
            cout<<"new mapping created "<< check->second<<" "<<check2->second <<endl;
            
            //get the new IP and ports
        } else {
        // mapping exists, directly retrieve new address
            modified_str = it->second;
            cout<<"retrieved new port "<<modified_str<<endl;
        } 
        final_dest_ip_and_port = dest_ip_string + " " +dest_port_string;
        final_source_ip_and_port = modified_str;
        //TODO: modify actual IP packet and TCP header (IP source, TCP source, IP checksum, TCP checksum)
        pair<uint32_t, uint16_t> modified_ip_and_port = parseIPAndPort(modified_str);
        modified_source_ip = modified_ip_and_port.first;
        modified_source_port = modified_ip_and_port.second;
        struct IP_header* modify_ip = (struct IP_header*)buffer;
        modify_ip->source_ip = htonl(modified_source_ip);
        uint8_t new_TTL = TTL-1;
        modify_ip->TTL = new_TTL;
        uint16_t modified_ip_checksum = compute_IP_checksum_value(modify_ip);
        modify_ip->ip_checksum = htons(modified_ip_checksum);
        cout<<"new ip is "<<ntohl(modify_ip->source_ip)<<" new checksum is "<<ntohs(modify_ip->ip_checksum)<<endl;
        if (protocol == 17){
            //modify udp header
            struct UDP_header* modify_udp = (struct UDP_header*)(buffer+header_len_bytes);
            modify_udp->source_port = htons(modified_source_port);
            uint16_t transport_header_length = sizeof(struct UDP_header);
            uint16_t actual_payload_length = payload_length - transport_header_length;
            uint16_t modified_udp_checksum = compute_UDP_checksum(modify_ip,modify_udp, buffer + header_len_bytes + transport_header_length, actual_payload_length);
            modify_udp->UDP_checksum = htons(modified_udp_checksum);

        }
        else if(protocol ==6){
            //modify tcp header
            struct TCP_header* modify_tcp = (struct TCP_header*)(buffer+header_len_bytes);
            modify_tcp->source_port = htons(modified_source_port);
            uint16_t transport_header_length = sizeof(struct TCP_header);
            uint16_t actual_payload_length = payload_length - transport_header_length;
            uint16_t modified_tcp_checksum = compute_TCP_checksum(modify_ip,modify_tcp, buffer + header_len_bytes + transport_header_length, actual_payload_length);
            modify_tcp->TCP_checksum = htons(modified_tcp_checksum);
        }
        else{
            perror("somethin is very wrong");
            return -1;
        }

        cout<<"finished modifying packet"<<endl;
        return 1;
    }

    //external to internal (both addresses are not local), use ext_NAT_table
    if (!source_is_lan){
        cout<<"this is external to internal forwarding, modificaion needed for destination"<<endl;
        //check if already in table
        auto it = ext_NAT_table.find(original_dest_str); 
        //if the mapping doesn't exist, create one
        string modified_str ="";
        if (it == ext_NAT_table.end()) {
            cout<<"entry does not exist yet, dropping"<<endl;
            return -1;            
        } else {
        // mapping exists, directly retrieve new address
            modified_str = it->second;
            cout<<"retrieved new port "<<modified_str<<endl;
        } 
        final_dest_ip_and_port = modified_str;
        final_source_ip_and_port = source_ip_string + " " +source_port_string;
        //TODO: modify actual IP packet and TCP header (IP source, TCP source, IP checksum, TCP checksum)
        pair<uint32_t, uint16_t> modified_ip_and_port = parseIPAndPort(modified_str);
        modified_dest_ip = modified_ip_and_port.first;
        modified_dest_port = modified_ip_and_port.second;

        struct IP_header* modify_ip = (struct IP_header*)buffer;
        modify_ip->dest_ip = htonl(modified_dest_ip);
        uint8_t new_TTL = TTL-1;
        modify_ip->TTL = new_TTL;
        uint16_t modified_ip_checksum = compute_IP_checksum_value(modify_ip);
        modify_ip->ip_checksum = htons(modified_ip_checksum);
        cout<<"new ip is "<<ntohl(modify_ip->dest_ip)<<" new checksum is "<<ntohs(modify_ip->ip_checksum)<<endl;
        if (protocol == 17){
            //modify udp header
            struct UDP_header* modify_udp = (struct UDP_header*)(buffer+header_len_bytes);
            modify_udp->dest_port = htons(modified_dest_port);
            uint16_t transport_header_length = sizeof(struct UDP_header);
            uint16_t actual_payload_length = payload_length - transport_header_length;
            uint16_t modified_udp_checksum = compute_UDP_checksum(modify_ip,modify_udp, buffer + header_len_bytes + transport_header_length, actual_payload_length);
            modify_udp->UDP_checksum = htons(modified_udp_checksum);

        }
        else if(protocol ==6){
            //modify tcp header
            struct TCP_header* modify_tcp = (struct TCP_header*)(buffer+header_len_bytes);
            modify_tcp->dest_port = htons(modified_dest_port);
            uint16_t transport_header_length = sizeof(struct TCP_header);
            uint16_t actual_payload_length = payload_length - transport_header_length;
            uint16_t modified_tcp_checksum = compute_TCP_checksum(modify_ip,modify_tcp, buffer + header_len_bytes + transport_header_length, actual_payload_length);
            modify_tcp->TCP_checksum = htons(modified_tcp_checksum);
        }
        else{
            perror("somethin is very wrong");
            return -1;
        }

        cout<<"finished modifying packet"<<endl;
        return 0;
    }
    perror("some thing wrong");
    cout<<"uh oh"<<endl;
    return -2;
}

void binaryStringToBytes(const char *binary, char *buffer, size_t len) {
    for (size_t i = 0; i < len; i += 8) {
        char temp[9] = {0};
        strncpy(temp, binary + i, 8);
        buffer[i / 8] = (char) strtol(temp, NULL, 2);
    }
}

