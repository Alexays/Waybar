#define _POSIX_C_SOURCE 200809L
#include "modules/sway/ipc/client.hpp"
#include <cstdio>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>

static const std::string ipc_magic("i3-ipc");
static const size_t ipc_header_size = ipc_magic.size() + 8;

std::string getSocketPath() {
  const char *env = getenv("SWAYSOCK");
  if (env != nullptr) {
    return std::string(env);
  }
  std::string str;
  {
    std::string str_buf;
    FILE*  in;
    char  buf[512] = { 0 };
    if ((in = popen("sway --get-socketpath 2>/dev/null", "r")) == nullptr) {
      throw std::runtime_error("Failed to get socket path");
    }
    while (fgets(buf, sizeof(buf), in) != nullptr) {
      str_buf.append(buf, sizeof(buf));
    }
    pclose(in);
    str = str_buf;
  }
  if (str.back() == '\n') {
    str.pop_back();
  }
  return str;
}

int ipcOpenSocket(const std::string &socketPath) {
  struct sockaddr_un addr = {0};
  int socketfd;
  if ((socketfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    throw std::runtime_error("Unable to open Unix socket");
  }
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);
  addr.sun_path[sizeof(addr.sun_path) - 1] = 0;
  int l = sizeof(struct sockaddr_un);
  if (connect(socketfd, reinterpret_cast<struct sockaddr *>(&addr), l) == -1) {
    throw std::runtime_error("Unable to connect to " + socketPath);
  }
  return socketfd;
}

struct ipc_response ipcRecvResponse(int socketfd) {
  std::string header;
  header.reserve(ipc_header_size);
  auto data32 = reinterpret_cast<uint32_t *>(header.data() + ipc_magic.size());
  size_t total = 0;

  while (total < ipc_header_size) {
    ssize_t res =
      ::recv(socketfd, header.data() + total, ipc_header_size - total, 0);
    if (res <= 0) {
      throw std::runtime_error("Unable to receive IPC response");
    }
    total += res;
  }

  total = 0;
  std::string payload;
  payload.reserve(data32[0]);
  while (total < data32[0]) {
    ssize_t res =
      ::recv(socketfd, payload.data() + total, data32[0] - total, 0);
    if (res < 0) {
      throw std::runtime_error("Unable to receive IPC response");
    }
    total += res;
  }
  return { data32[0], data32[1], &payload.front() };
}

struct ipc_response ipcSingleCommand(int socketfd, uint32_t type,
  const std::string& payload) {
  std::string header;
  header.reserve(ipc_header_size);
  auto data32 = reinterpret_cast<uint32_t *>(header.data() + ipc_magic.size());
  memcpy(header.data(), ipc_magic.c_str(), ipc_magic.size());
  data32[0] = payload.size();
  data32[1] = type;

  if (send(socketfd, header.data(), ipc_header_size, 0) == -1) {
    throw std::runtime_error("Unable to send IPC header");
  }
  if (send(socketfd, payload.c_str(), payload.size(), 0) == -1) {
    throw std::runtime_error("Unable to send IPC payload");
  }
  return ipcRecvResponse(socketfd);
}
