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
# include "functions.cpp"

#define DEFAULT_PORT 5152

#pragma pack(1)
struct IP_packet{
    uint4_t version;
    uint4_t header_length;
    uint16_t stuff;
    uint32_t more_stuff;
    uint64_t more_stuff2;
    uint8_t TTL;
    uint8_t protocol;
    uint16_t ip_checksum;
    uint32_t source_ip;
    uint32_t dest_ip;
}
#pragma pack()

int main() {
    std::string szLine;

    // First line is the router's LAN IP and the WAN IP
    std::getline(std::cin, szLine);
    size_t dwPos = szLine.find(' ');
    auto szLanIp = szLine.substr(0, dwPos);
    auto szWanIp = szLine.substr(dwPos + 1);

    std::cout << "Server's LAN IP: " << szLanIp << std::endl
            << "Server's WAN IP: " << szWanIp << std::endl;

    // TODO: Modify/Add/Delete files under the project folder.

    //--------------------------------------------------------------------------------------//
    // Client connection Setup:

    // Store IP address | port to socket pairings
    std::map<std::string, int> map;

    // Get num_clients from config input
    int num_clients, max_sd = 0;
    int listening_socket, client_sockets[num_clients];
    char buffer[1024];

    listening_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listening_socket == 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int addrlen;
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

    int connected_clients = 0;

    // Keep accepting connections until all clients are connected
    while(connected_clients < num_clients) {
        new_socket = accept(listening_socket, (struct sockaddr)*&address, (socklen_t *)&addrlen);

        if (new_socket < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
        }

        //Extract client address and port #
        //Some guy said using map to keep track of which socket corresponds to which address
        printf("Client connected, ip: %s, port: %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));

        // Go through socket array and fill in the next empty spot with new socket
        client_sockets[0] = new_socket;
        connected_clients++;
    }

    // Client connection setup end
    //--------------------------------------------------------------------------------------//
    // Poll connections with select() 

    while (1) {
        // Zero out fd set and then add all connections into it
        FD_ZERO(&readfds);
        for(int i = 0; i < num_clients; i++) {
            int sd = client_sockets[i];
            FD_SET(sd, &readfds);
            if (sd > max_sd) {
                max_sd = sd
            }
        }

        // Only select sockets with something to read
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL)
        if ((activity < 0) && (errno != EINTR)) {
            perror("select");
        }

        for (int i = 0; i < num_clients; i++) {
            int sd = client_sockets[i];

            if (FD_ISSET(sd, &readfds)) {
                // Receive IP packet from client if there is data
                valread = read(sd, buffer, BUFFER_SIZE);
                buffer[valread] = '\0';

                if (valread == 0) {
                    close(sd);
                    client_sockets[i] = 0;
                } else {
                    // Process the packet
                }
            }
        }
    }


    return 0;
}
