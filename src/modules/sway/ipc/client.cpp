#include "modules/sway/ipc/client.hpp"

waybar::modules::sway::Ipc::Ipc()
  : fd_(-1), fd_event_(-1)
{}

waybar::modules::sway::Ipc::~Ipc()
{
  close(fd_);
  close(fd_event_);
}

const std::string waybar::modules::sway::Ipc::getSocketPath() const
{
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

int waybar::modules::sway::Ipc::open(const std::string& socketPath) const
{
  struct sockaddr_un addr = {0};
  int fd = -1;
  if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    throw std::runtime_error("Unable to open Unix socket");
  }
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);
  addr.sun_path[sizeof(addr.sun_path) - 1] = 0;
  int l = sizeof(struct sockaddr_un);
  if (::connect(fd, reinterpret_cast<struct sockaddr *>(&addr), l) == -1) {
    throw std::runtime_error("Unable to connect to Sway");
  }
  return fd;
}

void waybar::modules::sway::Ipc::connect()
{
  const std::string& socketPath = getSocketPath();
  fd_ = open(socketPath);
  fd_event_ = open(socketPath);
}

struct waybar::modules::sway::Ipc::ipc_response
  waybar::modules::sway::Ipc::recv(int fd) const
{
  std::string header;
  header.reserve(ipc_header_size_);
  auto data32 = reinterpret_cast<uint32_t *>(header.data() + ipc_magic_.size());
  size_t total = 0;

  while (total < ipc_header_size_) {
    ssize_t res =
      ::recv(fd, header.data() + total, ipc_header_size_ - total, 0);
    if (res <= 0) {
      throw std::runtime_error("Unable to receive IPC response");
    }
    total += res;
  }

  total = 0;
  std::string payload;
  payload.reserve(data32[0] + 1);
  while (total < data32[0]) {
    ssize_t res =
      ::recv(fd, payload.data() + total, data32[0] - total, 0);
    if (res < 0) {
      throw std::runtime_error("Unable to receive IPC response");
    }
    total += res;
  }
  payload[data32[0]] = 0;
  return { data32[0], data32[1], &payload.front() };
}

struct waybar::modules::sway::Ipc::ipc_response
  waybar::modules::sway::Ipc::send(int fd, uint32_t type,
  const std::string& payload) const
{
  std::string header;
  header.reserve(ipc_header_size_);
  auto data32 = reinterpret_cast<uint32_t *>(header.data() + ipc_magic_.size());
  memcpy(header.data(), ipc_magic_.c_str(), ipc_magic_.size());
  data32[0] = payload.size();
  data32[1] = type;

  if (::send(fd, header.data(), ipc_header_size_, 0) == -1) {
    throw std::runtime_error("Unable to send IPC header");
  }
  if (::send(fd, payload.c_str(), payload.size(), 0) == -1) {
    throw std::runtime_error("Unable to send IPC payload");
  }
  return recv(fd);
}

struct waybar::modules::sway::Ipc::ipc_response
  waybar::modules::sway::Ipc::sendCmd(uint32_t type,
  const std::string& payload) const
{
  return send(fd_, type, payload);
}

void waybar::modules::sway::Ipc::subscribe(const std::string& payload) const
{
  auto res = send(fd_event_, IPC_SUBSCRIBE, payload);
  if (res.payload != "{\"success\": true}" && res.payload != "[{\"success\": true}]") {
    throw std::runtime_error("Unable to subscribe ipc event");
  }
}

struct waybar::modules::sway::Ipc::ipc_response
  waybar::modules::sway::Ipc::handleEvent() const
{
  return recv(fd_event_);
}
