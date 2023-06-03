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
#include <unordered_map>
#include "functions.cpp"


#define DEFAULT_PORT 5152
using namespace std;
#pragma pack(1)
struct IP_packet{
    uint8_t header_length :4;
    uint8_t version:4;
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

int main(){
    string header_length = "0101";
    string version = "0100";
    string stuff = "11111111";
    string total_length = "0000000000011100";
    string more_stuff2 = "11111111111111111111111111111111";
    string TTL = "00000001";
    string protocol = "00010001";
    string ip_checksum = "0000000000000000";
    string source_ip = "01100100000000011010100011000000";
    string dest_ip = "11001000000000011010100011000000";
    string source_port = "0001001110001000";
    string dest_port = "0001011101110000";
    string useless = "0000000000000000";
    string udp_checksum = "0110101001010111";
    string packet = header_length+version+ stuff+ total_length + more_stuff2+TTL+ protocol+ip_checksum+source_ip+dest_ip+ source_port+ dest_port+ useless + udp_checksum;

    //const char* test_header = "0100010111111111000000000001010000000000000000000000000000000000111111110000011011111111111111110000000000000000000000000000000011111111111111111111111111111111";
    const char* test_header = packet.c_str();
    //strcpy(test_header, packet.c_str());
    size_t len = strlen(test_header) / 8;  // length of binary string in bytes
    char* buffer = (char*) malloc(len);
    string dest_string = "";
    string source_string = "";
    binaryStringToBytes(test_header, buffer, strlen(test_header));
    const struct IP_header* test_ip =  (const struct IP_header*)buffer;
    uint16_t real_checksum_int = compute_IP_checksum_value(test_ip);
    cout<<"should be "<<real_checksum_int<<endl;
    string real_checksum = "0100111100101100";
    string real_packet = header_length+version+ stuff+ total_length + more_stuff2+TTL+ protocol + real_checksum + source_ip+dest_ip+source_port+dest_port+useless+udp_checksum;
    const char* test_header2 = real_packet.c_str();
    char* buffer2 = (char*) malloc(len);


    binaryStringToBytes(test_header2, buffer2, strlen(test_header2));


    /*
    uint8_t aheader_length = test_ip->header_length;
    int yah = aheader_length*4;
    printf("header length is %u\n", yah);
    const struct UDP_header* udp = (const struct UDP_header*)(buffer + yah);
    cout<<"in order "<<udp->source_port<<endl<<udp->dest_port<<endl<<udp->UDP_checksum<<endl;
    */


    processIPPacket(buffer2, len, dest_string, source_string);
    cout<<"final source IP and port: "<<source_string<<endl;
    cout<<"final destination IP and port: "<<dest_string<<endl;
    free(buffer);
    free(buffer2);
    return 0;
}
int main1() {
    //--------------------------------------------------------------------------------------//
    // Reading in Config file:

    std::string szLine;
    
    // First line is the router's LAN IP and the WAN IP
    std::getline(std::cin, szLine);
    size_t dwPos = szLine.find(' ');
    std::string router_LanIp = szLine.substr(0, dwPos);
    std::string router_WanIp = szLine.substr(dwPos + 1);
    // Skip the 2nd line
    std::getline(std::cin, szLine);
    
    // Store all local host IP addresses in order
    int num_clients = 0;
    std::string client_ips[30];    // Too lazy, just set some max client count

    // Step 1: Get all local host IP addresses
    while (1) {
        std::getline(std::cin, szLine);
        if (szLine.length() == 0) {
            break;
        }
        client_ips[num_clients] = szLine;
        num_clients++;

        // Debug use
        if (num_clients > 30) {
            perror("Max clients reached!");
            exit(1);
        }
    } 

    // Store internal IP|port to external IP|port mappings
    std::unordered_map<std::string, std::string> int_NAT_table;

    // Store external IP|port to internal IP|port mappings
    std::unordered_map<std::string, std::string> ext_NAT_table;

    // Step 2: Get static NAPT mappings
    while (1) {
        std::getline(std::cin, szLine);
        if (szLine.length() == 0) {
            break;
        }
        size_t ip = szLine.find(' ');
        std::string local_ip = szLine.substr(0, ip);
        size_t port = szLine.find(' ', ip + 1);
        std::string local_port = szLine.substr(ip + 1, port - ip - 1);
        std::string wan_port = szLine.substr(port + 1);

        std::string int_pair = local_ip + " " + local_port;
        std::string ext_pair = router_WanIp + " " + wan_port;

        int_NAT_table[int_pair] = ext_pair;
        ext_NAT_table[int_pair] = ext_pair;
    } 

    // Don't feel like implementing ACL for now and it's only 10 bonus

    // Config file processing end
    //--------------------------------------------------------------------------------------//
    // Client connections Setup:

    // Store IP address | port to socket pairings
    std::unordered_map<std::string, int> connection_map;

    // Socket to get connections and list of client sockets
    int listening_socket, client_sockets[num_clients];
    fd_set readfds;
    // Single ip packet maximum size
    char buffer[65535];

    listening_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listening_socket == 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(DEFAULT_PORT);

    // Bind socket to specified port 
    if (bind(listening_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections, max backlog specified as 10 in spec
    if (listen(listening_socket, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    int connection_num = 0;

    // Keep accepting connections until all clients are connected
    while(connection_num < num_clients) {
        int new_socket = accept(listening_socket, NULL, NULL);

        if (new_socket < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
        }

        // Add ip to socket mapping
        connection_map[client_ips[connection_num]] = new_socket;

        // Go through socket array and fill in the next empty spot with new socket
        client_sockets[connection_num] = new_socket;
        connection_num++;
    }

    // Client connection setup end
    //--------------------------------------------------------------------------------------//
    // Poll connections with select() 

}
