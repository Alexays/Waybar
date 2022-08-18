#include "modules/hypr/ipc.hpp"

#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "util/string.hpp"

std::string waybar::modules::hypr::makeRequest(std::string req) {
  const auto SERVERSOCKET = socket(AF_INET, SOCK_STREAM, 0);

  if (SERVERSOCKET < 0) {
    spdlog::error("[Hypr IPC] Couldn't open a socket.");
    return "";
  }

  const auto SERVER = gethostbyname("localhost");

  if (!SERVER) {
    spdlog::error("[Hypr IPC] Couldn't get localhost.");
    return "";
  }

  sockaddr_in serverAddress = {0};
  serverAddress.sin_family = AF_INET;
  bcopy((char*)SERVER->h_addr, (char*)&serverAddress.sin_addr.s_addr, SERVER->h_length);

  std::ifstream socketPortStream;
  socketPortStream.open("/tmp/hypr/.socket");

  if (!socketPortStream.good()) {
    spdlog::error("[Hypr IPC] No socket file. Is Hyprland running?");
    return "";
  }

  std::string port = "";
  std::getline(socketPortStream, port);
  socketPortStream.close();

  int portInt = 0;
  try {
    portInt = std::stoi(port.c_str());
  } catch (...) {
    spdlog::error("[Hypr IPC] Port not an int?!");
    return "";
  }

  if (portInt == 0) {
    spdlog::error("[Hypr IPC] Port 0. Aborting.");
    return "";
  }

  serverAddress.sin_port = portInt;

  if (connect(SERVERSOCKET, (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
    spdlog::error("[Hypr IPC] Couldn't connect to port {} , is Hyprland running?", port);
    return "";
  }

  auto sizeWritten = write(SERVERSOCKET, req.c_str(), req.length());

  if (sizeWritten < 0) {
    spdlog::error("[Hypr IPC] Couldn't write to the socket.");
    return "";
  }

  char buffer[8192] = {0};

  sizeWritten = read(SERVERSOCKET, buffer, 8192);

  if (sizeWritten < 0) {
    spdlog::error("[Hypr IPC] Couldn't cread from the socket.");
    return "";
  }

  close(SERVERSOCKET);

  return std::string(buffer);
}