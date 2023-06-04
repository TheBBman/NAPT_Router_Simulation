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
#define MAX_CLIENTS 10

//already defined in functions file
/*
#pragma pack(1)
struct IP_header{
    uint8_t version :4;
    uint8_t header_length :4;
    uint8_t stuff;
    uint16_t total_length;
    uint32_t more_stuff;
    uint8_t TTL;
    uint8_t protocol;
    uint16_t ip_checksum;
    uint32_t source_ip;
    uint32_t dest_ip;
};
#pragma pack()
*/


int main() {
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
    std::string client_ips[MAX_CLIENTS];    // Too lazy, just set some max client count

    // Step 1: Get all local host IP addresses
    while (1) {
        std::getline(std::cin, szLine);
        if (szLine.length() == 0) {
            break;
        }
        client_ips[num_clients] = szLine;
        num_clients++;

        // Debug use
        if (num_clients > MAX_CLIENTS) {
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
        ext_NAT_table[ext_pair] = int_pair;
    } 

    // Don't feel like implementing ACL for now and it's only 10 bonus

    // Config file processing end
    //--------------------------------------------------------------------------------------//
    // Client connections Setup:

    // Store IP address | port to socket pairings
    std::unordered_map<std::string, int> connection_map;

    // Socket to get connections and list of all sockets, client + WAN
    int num_sockets = num_clients + 1;
    int listening_socket, client_sockets[num_sockets];

    listening_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listening_socket == 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int optval = 1;                                         
    setsockopt(listening_socket, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)); 

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
    while(connection_num < num_sockets) {
        int new_socket = accept(listening_socket, NULL, NULL);

        if (new_socket < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
        }

        // Add ip to socket mapping, skip over first socket which is WAN
        if (connection_num > 0) {
            connection_map[client_ips[connection_num-1]] = new_socket;
        }

        // Go through socket array and fill in the next empty spot with new socket
        client_sockets[connection_num] = new_socket;
        connection_num++;
    }

    // Client connection setup end
    //--------------------------------------------------------------------------------------//
    // Poll connections with select() 

    // Needed for select(), highest socket # in the list
    int max_sd = 0;
    for(int i = 0; i < num_sockets; i++) {
        int sd = client_sockets[i];
        if (sd > max_sd) {
            max_sd = sd;
        }
    }

    // Single ip packet maximum size, to be reused for processing
    // Will only ever hold at most one ip packet, one buffer for each socket
    char buffer[num_sockets][65535];

    // Used to keep track of partial ip packets ^_^
    //int fragment[num_sockets] = {0};
    //int bytes_fragmented[num_sockets];

    // Used to keep track of dynamic NAT port allocation
    int dynamic_port = 49152;

    while (1) {
        // Zero out fd set and then add all connections into it
        fd_set readfds;
        FD_ZERO(&readfds);
        for(int i = 0; i < num_sockets; i++) {
            int sd = client_sockets[i];
            FD_SET(sd, &readfds);
        }

        // Only select sockets with something to read
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            perror("select");
        }

        // Loop through all client sockets
        for (int i = 0; i < num_sockets; i++) {
            int sd = client_sockets[i];

            // If socket was NOT selected, skip
            if (!FD_ISSET(sd, &readfds)) {
                continue;
            }

            // end of select()
            //--------------------------------------------------------------------------------------//
            // Extract single, valid ip packet

            //-----------------------------------------------------------------//
            // Fragmentation cases, will only happen once per select() so just hard code
            // Honestly I'll deal with fragmentation later when it happens

            /*
            // Fragmented header 
            if (fragment[i] == 1) {    
                int bytes = bytes_fragmented[i];

                // Read in next ip packet header
                int bytes_read = read(sd, buffer[i] + 20 - bytes, bytes);
                sd_read_head += bytes_read;

                // This means ip header fragmented, AGAIN
                if (bytes_read < 20) {
                    bytes_fragmented[i] = 20 - bytes - bytes_read;
                    continue;
                }
                // Normal logic

                // Find total length of ip packet
                const struct IP_header* ip = (const struct IP_header*)buffer[i];
                uint16_t payload_length = ntohs(ip->total_length) - 20;
                
                bytes_read = read(sd, buffer[i] + sd_read_head, payload_length);
                
                // This means ip payload fragmented (bruh)
                if (bytes_read < payload_length) {
                    fragment[i] = 2;
                    bytes_fragmented[i] = payload_length - bytes_read;
                    continue;
                }

                // Now buffer should contain a full ip packet

                // ---> Randy doing his magic <--- //

                std::string final_source_ip_port;
                std::string final_dest_ip_port;
                int result = processIPPacket(buffer[i], 0, final_source_ip_port, final_dest_ip_port, int_NAT_table, ext_NAT_table, router_LanIp, router_WanIp, dynamic_port);
                if (result < 0) {
                    // Drop the packet
                    continue;
                }

                size_t pos = final_dest_ip_port.find(' ');
                std::string final_dest_ip = final_dest_ip_port.substr(0, pos);
                int dest_socket = connection_map[final_dest_ip];
                write(dest_socket, buffer[i], payload_length + 20);

                // Reset buffer back to known state
                fragment[i] = 0;
            }

            // Fragmented payload 
            else if (fragment[i] == 2) {
                std::cout << "frag2 has been called" << std::endl;

                const struct IP_header* ip = (const struct IP_header*)buffer[i];
                uint16_t payload_length = ntohs(ip->total_length) - 20;

                int buf_position = payload_length + 20 - bytes_fragmented[i];
                
                int bytes_read = read(sd, buffer[i] + buf_position, bytes_fragmented[i]);
                
                // This means ip payload fragmented, again
                if (bytes_read < payload_length - bytes_fragmented[i]) {
                    std::cout << "fragmented again?" << std::endl;
                    bytes_fragmented[i] = payload_length - bytes_read;
                    continue;
                }

                sd_read_head += bytes_read;

                // Now buffer should contain a full ip packet

                // ---> Randy doing his magic <--- //

                std::string final_source_ip_port;
                std::string final_dest_ip_port;
                int result = processIPPacket(buffer[i], 0, final_source_ip_port, final_dest_ip_port, int_NAT_table, ext_NAT_table, router_LanIp, router_WanIp, dynamic_port);
                if (result < 0) {
                    // Drop the packet
                    continue;
                }

                size_t pos = final_dest_ip_port.find(' ');
                std::string final_dest_ip = final_dest_ip_port.substr(0, pos);
                
                int dest_socket;
                auto it = connection_map.find(final_dest_ip);
                if (it == connection_map.end()) {
                    dest_socket = client_sockets[0];
                } else {
                    dest_socket = it->second;
                }
                write(dest_socket, buffer[i], payload_length + 20);

                // Reset buffer back to known state
                fragment[i] = 0;
            }
            */

            //-----------------------------------------------------------------//
            // Debugging
            int loop_count = 0;

            // Could be multiple packets in stream
            while(1) {
                // Read in next ip packet header
                int bytes_read = read(sd, buffer[i], 20);

                // This means nothing more to read
                if (bytes_read == 0) {
                    break;
                }

                // This means ip header fragmented 
                if (bytes_read < 20) {
                    std::cout << "fragment case 1" << std::endl;
                    //fragment[i] = 1;
                    //bytes_fragmented[i] = 20 - bytes_read;
                    break;
                }
                // Normal logic

                // Find total length of ip packet
                const struct IP_header* ip = (const struct IP_header*)buffer[i];
                uint16_t payload_length = ntohs(ip->total_length) - 20;
                
                bytes_read = read(sd, buffer[i] + 20, payload_length);

                // This means ip payload fragmented
                if (bytes_read < payload_length) {
                    std::cout << "fragment case 2" << std::endl;
                    //fragment[i] = 2;
                    //bytes_fragmented[i] = payload_length - bytes_read;
                    break;
                }

                // Now buffer should contain a full ip packet

                // ---> Randy doing his magic, process IP packet <--- //

                // What to return:

                // Drop or forward? If drop, just skip the following forwarding section
                // For forwarding section:
                // Any modifications? Recomputing checksum?
                // If source is from outside, search in ext_NAT_table. If not found, drop
                // Obtain destination ip. If in connection_map, local. If not found, send to WAN
                // Static or Dynamic NAPT? if doing dynamic NAPT, add pairing to both NAPT maps
                // Write to specified socket

                std::string final_source_ip_port;
                std::string final_dest_ip_port;
                int result = processIPPacket(buffer[i], 0, final_source_ip_port, final_dest_ip_port, int_NAT_table, ext_NAT_table, router_LanIp, router_WanIp, dynamic_port);
                if (result < 0) {
                    // Drop the packet
                    std::cout << "packet dropped" << std::endl;
                    continue;
                }

                size_t pos = final_dest_ip_port.find(' ');
                std::string final_dest_ip = final_dest_ip_port.substr(0, pos);

                int dest_socket;
                auto it = connection_map.find(final_dest_ip);
                if (it == connection_map.end()) {
                    dest_socket = client_sockets[0];
                } else {
                    dest_socket = it->second;
                }
                write(dest_socket, buffer[i], payload_length + 20);
                // Remember to copy and paste this code to the two fragmentation cases at the end too
            }   
        }
    }
}

