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

#define DEFAULT_PORT 5152
#define MAX_CLIENTS 10

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

    // Needed for select(), highest socket # in the list
    int max_sd = 0;

    // Single ip packet maximum size, to be reused for processing
    // Will only ever hold at most one ip packet
    char buffer[num_clients][65535];

    // Used to keep track of partial ip packets ^_^
    int fragment[num_clients] = {0};
    int bytes_fragmented[num_clients];

    while (1) {
        // Zero out fd set and then add all connections into it
        FD_ZERO(&readfds);
        for(int i = 0; i < num_clients; i++) {
            int sd = client_sockets[i];
            FD_SET(sd, &readfds);
            if (sd > max_sd) {
                max_sd = sd;
            }
        }

        // Only select sockets with something to read
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            perror("select");
        }

        // Loop through all client sockets
        for (int i = 0; i < num_clients; i++) {
            int sd = client_sockets[i];

            // If socket was NOT selected, skip
            if (!FD_ISSET(sd, &readfds)) {
                continue;
            }

            // end of select()
            //--------------------------------------------------------------------------------------//
            // Extract single, valid ip packet

            // How many bytes into available stream has been read
            int sd_read_head = 0;

            //-----------------------------------------------------------------//
            // Fragmentation cases, will only happen once per select() so just hard code

            // Fragmented header 
            if (fragment[i] == 1) {
                int bytes = bytes_to_read[i];

                // Read in next ip packet header
                int bytes_read = read(sd, buffer[i] + 20 - bytes, bytes);
                sd_read_head += bytes_read;

                // This means ip header fragmented, AGAIN
                else if (bytes_read < 20) {
                    fragment[i] = 1;
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

                // Reset buffer back to known state
                memset(buffer[i], 0, 65535);
            }
            // Fragmented payload 
            else if (fragment[i] == 2) {
                const struct IP_header* ip = (const struct IP_header*)buffer[i];
                uint16_t payload_length = ntohs(ip->total_length) - 20;

                int head_position = payload_length - bytes_to_read[i]
                
                int bytes_read = read(sd, buffer[i] + head_position, bytes_to_read[i]);
                
                // This means ip payload fragmented, again
                if (bytes_read < payload_length) {
                    fragment[i] = 2;
                    bytes_fragmented[i] = payload_length - bytes_read;
                    continue;
                }

                sd_read_head += bytes_read;

                // Now buffer should contain a full ip packet

                // ---> Randy doing his magic <--- //

                // Reset buffer back to known state
                memset(buffer[i], 0, 65535);
            }

            //-----------------------------------------------------------------//

            // Could be multiple packets in stream
            while(1) {
                // Read in next ip packet header
                int bytes_read = read(sd, buffer[i] + sd_read_head, 20);
                sd_read_head += bytes_read;

                // This probably means test is over, wait for SIGKILL
                if (bytes_read == 0) {
                    break;;
                }
                // This means ip header fragmented 
                else if (bytes_read < 20) {
                    fragment[i] = 1;
                    bytes_fragmented[i] = 20 - bytes_read;
                    break;
                }
                // Normal logic

                // Find total length of ip packet
                const struct IP_header* ip = (const struct IP_header*)buffer[i];
                uint16_t payload_length = ntohs(ip->total_length) - 20;
                
                bytes_read = read(sd, buffer[i] + sd_read_head, payload_length);
                
                // This means ip payload fragmented
                if (bytes_read < payload_length) {
                    fragment[i] = 2;
                    bytes_fragmented[i] = payload_length - bytes_read;
                    break;
                }

                // Now buffer should contain a full ip packet

                // ---> Randy doing his magic <--- //

                // Reset buffer back to known state
                memset(buffer[i], 0, 65535);
            }
        }
    }
}
