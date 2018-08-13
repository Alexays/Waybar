#define _POSIX_C_SOURCE 200809L
#include <string>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "ipc/client.hpp"

static const char ipc_magic[] = {'i', '3', '-', 'i', 'p', 'c'};
static const size_t ipc_header_size = sizeof(ipc_magic)+8;

std::string get_socketpath(void) {
  const char *env = getenv("SWAYSOCK");
  if (env) return std::string(env);
  std::string str;
  {
    std::string str_buf;
    FILE*  in;
    char  buf[512] = { 0 };
    if (!(in = popen("sway --get-socketpath 2>/dev/null", "r"))) {
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

int ipc_open_socket(std::string socket_path) {
  struct sockaddr_un addr;
  int socketfd;
  if ((socketfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    throw std::runtime_error("Unable to open Unix socket");
  }
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
  addr.sun_path[sizeof(addr.sun_path) - 1] = 0;
  int l = sizeof(struct sockaddr_un);
  if (connect(socketfd, (struct sockaddr *)&addr, l) == -1) {
    throw std::runtime_error("Unable to connect to " + socket_path);
  }
  return socketfd;
}

struct ipc_response ipc_recv_response(int socketfd) {
  struct ipc_response response;
  char data[ipc_header_size];
  uint32_t *data32 = (uint32_t *)(data + sizeof(ipc_magic));
  size_t total = 0;

  while (total < ipc_header_size) {
    ssize_t received = recv(socketfd, data + total, ipc_header_size - total, 0);
    if (received <= 0) {
      throw std::runtime_error("Unable to receive IPC response");
    }
    total += received;
  }

  total = 0;
  response.size = data32[0];
  response.type = data32[1];
  char payload[response.size + 1];

  while (total < response.size) {
    ssize_t received = recv(socketfd, payload + total, response.size - total, 0);
    if (received < 0) {
      throw std::runtime_error("Unable to receive IPC response");
    }
    total += received;
  }
  payload[response.size] = '\0';
  response.payload = std::string(payload);
  return response;
}

std::string ipc_single_command(int socketfd, uint32_t type, const char *payload, uint32_t *len) {
  char data[ipc_header_size];
  uint32_t *data32 = (uint32_t *)(data + sizeof(ipc_magic));
  memcpy(data, ipc_magic, sizeof(ipc_magic));
  data32[0] = *len;
  data32[1] = type;

  if (send(socketfd, data, ipc_header_size, 0) == -1)
    throw std::runtime_error("Unable to send IPC header");
  if (send(socketfd, payload, *len, 0) == -1)
    throw std::runtime_error("Unable to send IPC payload");
  struct ipc_response resp = ipc_recv_response(socketfd);
  *len = resp.size;
  return resp.payload;
}
