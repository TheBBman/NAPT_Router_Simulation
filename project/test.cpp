int test(){
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

    string header_length = "0101";
    string version = "0100";
    string stuff = "11111111";
    string total_length = "0000000000011100";
    string more_stuff2 = "11111111111111111111111111111111";
    string TTL = "00000010";
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
    string real_checksum = "0100111100101011";
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

    int cur_unused_port = 49152;
    processIPPacket(buffer2, len, source_string, dest_string, int_NAT_table, ext_NAT_table, router_LanIp, router_WanIp, cur_unused_port);
    cout<<"final source IP and port: "<<source_string<<endl;
    cout<<"final destination IP and port: "<<dest_string<<endl;
    const struct IP_header* modified_ip = (const struct IP_header*)buffer2;
    cout<<ntohl(modified_ip->dest_ip)<<" "<<ntohs(modified_ip->ip_checksum)<<" "<<unsigned(modified_ip->TTL)<<endl;
    const struct UDP_header* modified_udp = (const struct UDP_header*)(buffer2 + (modified_ip->header_length*4));
    cout<<ntohs(modified_udp->dest_port)<<" "<<ntohs(modified_udp->UDP_checksum)<<endl;
    free(buffer);
    free(buffer2);
    return 0;
}