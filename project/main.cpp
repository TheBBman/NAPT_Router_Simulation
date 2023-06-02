#include <iostream>
#include <string>
# include "functions.cpp"
using namespace std;
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
  std::string test_header = "0100010111111111000000000001010000000000000000000000000000000000111111110000011011111111111111110000000000000000000000000000000011111111111111111111111111111111";
  std::cout <<test_header<<std::endl;
  return 0;
}
