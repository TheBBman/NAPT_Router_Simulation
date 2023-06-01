#include <iostream>
#include <string>

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

  return 0;
}
