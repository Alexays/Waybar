#include "modules/triad/backend.hpp"

#include <netdb.h>
#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <string>
#include <thread>

#include "giomm/datainputstream.h"
#include "giomm/dataoutputstream.h"
#include "giomm/unixinputstream.h"
#include "giomm/unixoutputstream.h"
#include "util/scoped_fd.hpp"

namespace waybar::modules::triad {
namespace {

constexpr int kTriadIpcVersion = 1;

Json::Value triadRequest(const std::string& request) {
  Json::Value root(Json::objectValue);
  auto& triad = (root["triad"] = Json::Value(Json::objectValue));
  triad["version"] = kTriadIpcVersion;
  triad["request"] = request;
  return root;
}

Json::Value triadPayload(const Json::Value& response) {
  if (!response["ok"].asBool()) {
    throw std::runtime_error(response["error"].asString());
  }

  const auto& payload = response["triad"];
  if (!payload || payload["version"].asInt() != kTriadIpcVersion) {
    throw std::runtime_error("invalid Triad IPC response");
  }

  return payload;
}

bool sortByOutputAndIndex(const Json::Value& a, const Json::Value& b) {
  const auto& aOutput = a["output"].asString();
  const auto& bOutput = b["output"].asString();
  const auto aIdx = a["workspace_idx"].asUInt();
  const auto bIdx = b["workspace_idx"].asUInt();
  if (aOutput == bOutput) return aIdx < bIdx;
  return aOutput < bOutput;
}

std::string serializeRequest(const Json::Value& request) {
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
  std::ostringstream oss;
  writer->write(request, &oss);
  oss << '\n';
  return oss.str();
}

}  // namespace

IPC::IPC() {
  loadState();
  startIPC();
}

int IPC::connectToSocket() {
  const char* socket_path = getenv("TRIAD_SOCKET");
  std::string fallback_path;

  if (socket_path == nullptr || socket_path[0] == '\0') {
    const char* runtime_dir = getenv("XDG_RUNTIME_DIR");
    fallback_path =
        std::string(runtime_dir != nullptr && runtime_dir[0] != '\0' ? runtime_dir : "/tmp") +
        "/triad.sock";
    socket_path = fallback_path.c_str();
  }

  struct sockaddr_un addr;
  util::ScopedFd socketfd(socket(AF_UNIX, SOCK_STREAM, 0));

  if (socketfd == -1) {
    throw std::runtime_error("socketfd failed");
  }

  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
  addr.sun_path[sizeof(addr.sun_path) - 1] = 0;

  int l = sizeof(struct sockaddr_un);

  if (connect(socketfd, (struct sockaddr*)&addr, l) == -1) {
    throw std::runtime_error("unable to connect to Triad IPC socket");
  }

  return socketfd.release();
}

Json::Value IPC::request(const std::string& name) { return send(triadRequest(name)); }

void IPC::loadState() {
  const auto payload = triadPayload(request("state"));
  parseState(payload["state"]);
}

void IPC::startIPC() {
  int socketfd = connectToSocket();

  std::thread([this, socketfd]() {
    spdlog::info("Triad IPC starting");

    auto unix_istream = Gio::UnixInputStream::create(socketfd, true);
    auto unix_ostream = Gio::UnixOutputStream::create(socketfd, false);
    auto istream = Gio::DataInputStream::create(unix_istream);
    auto ostream = Gio::DataOutputStream::create(unix_ostream);

    Json::Value request(Json::objectValue);
    auto& triad = (request["triad"] = Json::Value(Json::objectValue));
    triad["version"] = kTriadIpcVersion;
    triad["request"] = "event-stream";
    auto& events = (triad["events"] = Json::Value(Json::arrayValue));
    events.append("state");
    events.append("layout");
    events.append("window");

    if (!ostream->put_string(serializeRequest(request)) || !ostream->flush()) {
      spdlog::error("Triad IPC: failed to start event stream");
      return;
    }

    std::string line;
    while (istream->read_line(line)) {
      spdlog::debug("Triad IPC: received {}", line);

      try {
        parseIPC(line);
      } catch (std::exception& e) {
        spdlog::warn("Failed to parse Triad IPC message: {}, reason: {}", line, e.what());
      } catch (...) {
        throw;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }).detach();
}

void IPC::parseIPC(const std::string& line) {
  const auto root = parser_.parse(line);
  if (root["ok"].isBool()) {
    if (!root["ok"].asBool()) {
      spdlog::error("Triad IPC error: {}", root["error"].asString());
    }
    return;
  }

  const auto& payload = root["triad"];
  if (!payload || payload["version"].asInt() != kTriadIpcVersion) {
    throw std::runtime_error("invalid Triad IPC event");
  }

  const auto event = payload["event"].asString();
  if (event == "state-changed") {
    parseState(payload["state"]);
  } else if (event == "layout-state-changed") {
    auto lock = lockData();
    parseLayoutState(payload["state"]);
  } else if (event == "window-changed") {
    auto lock = lockData();
    parseWindowChanged(payload["window"]);
  } else {
    return;
  }

  std::unique_lock lock(callbackMutex_);
  for (auto& [eventname, handler] : callbacks_) {
    if (eventname == event) {
      handler->onEvent(payload);
    }
  }
}

void IPC::parseState(const Json::Value& state) {
  auto lock = lockData();

  parseLayoutState(state["layout"]);

  outputs_.clear();
  const auto& outputs = state["outputs"];
  std::copy(outputs.begin(), outputs.end(), std::back_inserter(outputs_));

  windows_.clear();
  const auto& windows = state["windows"];
  std::copy(windows.begin(), windows.end(), std::back_inserter(windows_));

  keyboardLayoutNames_.clear();
  const auto& names = state["keyboard_layouts"];
  for (const auto& name : names) keyboardLayoutNames_.push_back(name.asString());
  keyboardLayoutCurrent_ = state["current_keyboard_layout_idx"].asUInt();
}

void IPC::parseLayoutState(const Json::Value& state) {
  workspaces_.clear();
  const auto& workspaces = state["workspaces"];
  std::copy(workspaces.begin(), workspaces.end(), std::back_inserter(workspaces_));
  std::sort(workspaces_.begin(), workspaces_.end(), sortByOutputAndIndex);
}

void IPC::parseWindowChanged(const Json::Value& window) {
  const auto id = window["id"].asUInt64();
  auto it = std::find_if(windows_.begin(), windows_.end(),
                         [id](const auto& win) { return win["id"].asUInt64() == id; });
  if (it == windows_.end()) {
    windows_.push_back(window);
  } else {
    *it = window;
  }
}

void IPC::registerForIPC(const std::string& ev, EventHandler* ev_handler) {
  if (ev_handler == nullptr) {
    return;
  }

  std::unique_lock lock(callbackMutex_);
  callbacks_.emplace_back(ev, ev_handler);
}

void IPC::unregisterForIPC(EventHandler* ev_handler) {
  if (ev_handler == nullptr) {
    return;
  }

  std::unique_lock lock(callbackMutex_);
  for (auto it = callbacks_.begin(); it != callbacks_.end();) {
    auto& [eventname, handler] = *it;
    if (handler == ev_handler) {
      it = callbacks_.erase(it);
    } else {
      ++it;
    }
  }
}

Json::Value IPC::send(const Json::Value& request) {
  util::ScopedFd socketfd(connectToSocket());

  auto unix_istream = Gio::UnixInputStream::create(socketfd, true);
  auto unix_ostream = Gio::UnixOutputStream::create(socketfd, false);
  auto istream = Gio::DataInputStream::create(unix_istream);
  auto ostream = Gio::DataOutputStream::create(unix_ostream);

  if (!ostream->put_string(serializeRequest(request)) || !ostream->flush()) {
    throw std::runtime_error("error writing to Triad socket");
  }

  std::string line;
  if (!istream->read_line(line)) throw std::runtime_error("error reading from Triad socket");

  std::istringstream iss(std::move(line));
  Json::Value response;
  iss >> response;
  return response;
}

Json::Value IPC::action(const std::string& action_name) {
  Json::Value request = triadRequest("action");
  request["triad"]["action"] = action_name;
  return triadPayload(send(request));
}

Json::Value IPC::action(const std::string& action_name, const std::string& key,
                        const Json::Value& value) {
  Json::Value request = triadRequest("action");
  request["triad"]["action"] = action_name;
  request["triad"][key] = value;
  return triadPayload(send(request));
}

}  // namespace waybar::modules::triad
