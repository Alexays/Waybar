#include "modules/niri/backend.hpp"

#include <netdb.h>
#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <string>
#include <thread>

#include "giomm/datainputstream.h"
#include "giomm/dataoutputstream.h"
#include "giomm/unixinputstream.h"
#include "giomm/unixoutputstream.h"

#define JSON_GET_OR_RET(newvar, obj, key) \
  const Json::Value & newvar = obj[key]; \
  if (newvar.isNull()) { \
    spdlog::debug("When setting {}, {}['{}'] returned null", "#newvar", "#obj", "#key"); \
    return false; \
  }

namespace waybar::modules::niri {

int IPC::connectToSocket() {
  const char *socket_path = getenv("NIRI_SOCKET");

  if (socket_path == nullptr) {
    spdlog::warn("Niri is not running, niri IPC will not be available.");
    return -1;
  }

  struct sockaddr_un addr;
  int socketfd = socket(AF_UNIX, SOCK_STREAM, 0);

  if (socketfd == -1) {
    throw std::runtime_error("socketfd failed");
  }

  addr.sun_family = AF_UNIX;

  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

  addr.sun_path[sizeof(addr.sun_path) - 1] = 0;

  int l = sizeof(struct sockaddr_un);

  if (connect(socketfd, (struct sockaddr *)&addr, l) == -1) {
    close(socketfd);
    throw std::runtime_error("unable to connect");
  }

  return socketfd;
}

void IPC::startIPC() {
  // will start IPC and relay events to parseIPC

  std::thread([&]() {
    int socketfd;
    try {
      socketfd = connectToSocket();
    } catch (std::exception &e) {
      spdlog::error("Niri IPC: failed to start, reason: {}", e.what());
      return;
    }
    if (socketfd == -1) return;

    spdlog::info("Niri IPC starting");

    auto unix_istream = Gio::UnixInputStream::create(socketfd, true);
    auto unix_ostream = Gio::UnixOutputStream::create(socketfd, false);
    auto istream = Gio::DataInputStream::create(unix_istream);
    auto ostream = Gio::DataOutputStream::create(unix_ostream);

    if (!ostream->put_string("\"EventStream\"\n") || !ostream->flush()) {
      spdlog::error("Niri IPC: failed to start event stream");
      return;
    }

    std::string line;
    if (!istream->read_line(line) || line != R"({"Ok":"Handled"})") {
      spdlog::error("Niri IPC: failed to start event stream");
      return;
    }

    while (istream->read_line(line)) {
      spdlog::debug("Niri IPC: received {}", line);

      try {
        parseIPC(line);
      } catch (std::exception &e) {
        spdlog::warn("Failed to parse IPC message: {}, reason: {}", line, e.what());
      } catch (...) {
        throw;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }).detach();
}

bool IPC::parseWorkspacesChanged_(const Json::Value &payload) {
  workspaces_.clear();
  JSON_GET_OR_RET(values, payload, "workspaces")

  std::ranges::copy(values, std::back_inserter(workspaces_));

  std::ranges::sort(workspaces_, [](const auto &a, const auto &b) {
    const auto &aOutput = a["output"].asString();
    const auto &bOutput = b["output"].asString();
    const auto aIdx = a["idx"].asUInt();
    const auto bIdx = b["idx"].asUInt();
    if (aOutput == bOutput) return aIdx < bIdx;
    return aOutput < bOutput;
  });

  return true;
}

bool IPC::parseWorkspaceActivated_(const Json::Value &payload) {
  JSON_GET_OR_RET(id_obj, payload, "id");
  const auto id = id_obj.asUInt64();

  JSON_GET_OR_RET(focused_obj, payload, "focused");
  const auto focused = focused_obj.asBool();

  auto it = std::ranges::find_if(workspaces_, [id](const auto &ws) {
      return ws["id"].asUInt64() == id;
  });

  if (it != workspaces_.end()) {
    JSON_GET_OR_RET(output_obj, (*it), "output");
    const auto &output = output_obj.asString();
    for (auto &ws : workspaces_) {
      const auto got_activated = (ws["id"].asUInt64() == id);
      if (ws["output"] == output) {
        ws["is_active"] = got_activated;
      }
      if (focused) {
        ws["is_focused"] = got_activated;
      }
    }
  } else {
    spdlog::error("Activated unknown workspace");
    return false;
  }

  return true;
}

bool IPC::parseWorkspaceActiveWindowChanged_(const Json::Value &payload) {
  JSON_GET_OR_RET(workspace_id_obj, payload, "workspace_id");
  const auto workspace_id = workspace_id_obj.asUInt64();

  auto it = std::ranges::find_if(workspaces_, [workspace_id](const auto &ws) {
    return ws["id"].asUInt64() == workspace_id;
  });
  if (it != workspaces_.end()) {
    auto &ws = *it;
    ws["active_window_id"] = payload["active_window_id"];
  } else {
    spdlog::error("Active window changed on unknown workspace");
    return false;
  }

  return true;
}

bool IPC::parseWorkspaceUrgencyChanged_(const Json::Value &payload) {
  JSON_GET_OR_RET(id_obj, payload, "id");
  const auto id = id_obj.asUInt64();
  JSON_GET_OR_RET(urgent_obj, payload, "urgent");
  const auto urgent = urgent_obj.asBool();

  auto it = std::ranges::find_if(workspaces_, [id](const auto &ws) {
    return ws["id"].asUInt64() == id;
  });

  if (it != workspaces_.end()) {
    auto &ws = *it;
    ws["is_urgent"] = urgent;
  } else {
    spdlog::error("Urgency changed for unknown workspace");
    return false;
  }
  return true;
}

bool IPC::parseKeyboardLayoutsChanged_(const Json::Value &payload) {
  JSON_GET_OR_RET(layouts_obj, payload, "keyboard_layouts");
  JSON_GET_OR_RET(names_obj, layouts_obj, "names");
  JSON_GET_OR_RET(keyboard_layout_current_obj, layouts_obj, "current_idx");
  keyboardLayoutCurrent_ = keyboard_layout_current_obj.asUInt();

  keyboardLayoutNames_.clear();
  for (const auto &fullName : names_obj) keyboardLayoutNames_.push_back(fullName.asString());
  return true;
}

bool IPC::parseKeyboardLayoutsSwitched_(const Json::Value &payload) {
  JSON_GET_OR_RET(keyboard_layout_current_obj, payload, "idx");
  keyboardLayoutCurrent_ = keyboard_layout_current_obj.asUInt();
  return true;
}

bool IPC::parseWindowsChanged_(const Json::Value &payload) {
  JSON_GET_OR_RET(windows_obj, payload, "windows");
  windows_.clear();
  // XXX This copies the newly RX'd windows into the `windows_` global, but
  // doesn't validate their contents.
  std::ranges::copy(windows_obj, std::back_inserter(windows_));
  return true;
}

bool IPC::parseWindowLayoutsChanged_(const Json::Value &payload) {
  JSON_GET_OR_RET(changes_obj, payload, "changes");
  // Expect Vec<(u64, WindowLayout)>
  for (const auto &win_changes : changes_obj) {
    for (auto &win : windows_) {
      JSON_GET_OR_RET(win_changes_id_obj, win_changes, 0);
      if (win["id"] == win_changes_id_obj) {
        JSON_GET_OR_RET(win_layout_obj, win_changes, 1);
        JSON_GET_OR_RET(win_layout_scrolling_pos_obj, win_layout_obj, "pos_in_scrolling_layout");
        // XXX We construct a new object with selective copying so that our consumer modules can
        //     easily ignore unimportant changes.
        win["layout"] = Json::Value(Json::objectValue);
        win["layout"]["pos_in_scrolling_layout"] = win_layout_scrolling_pos_obj;
        break;
      }
    }
  }
  return true;
}

bool IPC::parseWindowOpenedOrChanged_(const Json::Value &payload) {
  JSON_GET_OR_RET(window, payload, "window");
  JSON_GET_OR_RET(window_id_obj, window, "id");
  const auto id = window_id_obj.asUInt64();

  auto it = std::ranges::find_if(windows_, [id](const auto &win) {
    return win["id"].asUInt64() == id;
  });

  // Check if window from IPC is a new window.
  if (it == windows_.end()) {
    windows_.push_back(window);

    // Since new window, update all existing windows "is_focused" state.
    JSON_GET_OR_RET(window_focused_obj, window, "is_focused");
    if (window_focused_obj.asBool()) {
      for (auto &existing_win : windows_) {
        existing_win["is_focused"] = (existing_win["id"].asUInt64() == id);
      }
    }
  } else {
    // XXX Copies in input window json data into windows_ without checking.
    *it = window;
  }
  return true;
}

bool IPC::parseWindowClosed_(const Json::Value &payload) {
  JSON_GET_OR_RET(id_obj, payload, "id");
  const auto id = id_obj.asUInt64();

  auto it = std::ranges::find_if(windows_, [id](const auto &win) {
    return win["id"].asUInt64() == id;
  });

  if (it != windows_.end()) {
    windows_.erase(it);
  } else {
    spdlog::error("Unknown window closed");
    return false;
  }
  return true;
}

bool IPC::parseWindowFocusChanged_(const Json::Value &payload) {
  const auto focused = !payload["id"].isNull();
  const auto id = payload["id"].asUInt64();
  for (auto &win : windows_) {
    win["is_focused"] = focused && win["id"].asUInt64() == id;
  }
  return true;
}

void IPC::parseIPC(const std::string &line) {
  bool parsed = true;
  bool parse_ok = false;
  const auto ev = parser_.parse(line);
  const auto members = ev.getMemberNames();
  if (members.size() != 1) {
    throw std::runtime_error("Event must have a single member");
  }

  {
    auto lock = lockData();

    if (const auto &payload = ev["WorkspacesChanged"]) {
      parse_ok = this->parseWorkspacesChanged_(payload);
    } else if (const auto &payload = ev["WorkspaceActivated"]) {
      parse_ok = this->parseWorkspaceActivated_(payload);
    } else if (const auto &payload = ev["WorkspaceActiveWindowChanged"]) {
      parse_ok = this->parseWorkspaceActiveWindowChanged_(payload);
    } else if (const auto &payload = ev["WorkspaceUrgencyChanged"]) {
      parse_ok = this->parseWorkspaceUrgencyChanged_(payload);
    } else if (const auto &payload = ev["KeyboardLayoutsChanged"]) {
      parse_ok = this->parseKeyboardLayoutsChanged_(payload);
    } else if (const auto &payload = ev["KeyboardLayoutSwitched"]) {
      parse_ok = this->parseKeyboardLayoutsSwitched_(payload);
    } else if (const auto &payload = ev["WindowsChanged"]) {
      parse_ok = this->parseWindowsChanged_(payload);
    } else if (const auto &payload = ev["WindowLayoutsChanged"]) {
      parse_ok = this->parseWindowLayoutsChanged_(payload);
    } else if (const auto &payload = ev["WindowOpenedOrChanged"]) {
      parse_ok = this->parseWindowOpenedOrChanged_(payload);
    } else if (const auto &payload = ev["WindowClosed"]) {
      parse_ok = this->parseWindowClosed_(payload);
    } else if (const auto &payload = ev["WindowFocusChanged"]) {
      parse_ok = this->parseWindowFocusChanged_(payload);
    } else {
      parsed = false;
    }
  }

  if (parsed && (!parse_ok)) {
    spdlog::error("Niri IPC: Error parsing IPC: {}", ev);
    throw std::runtime_error("Message parser returned an error");
  }

  std::unique_lock lock(callbackMutex_);

  for (auto &[eventname, handler] : callbacks_) {
    if (eventname == members[0]) {
      handler->onEvent(ev);
    }
  }
}

void IPC::registerForIPC(const std::string &ev, EventHandler *ev_handler) {
  if (ev_handler == nullptr) {
    return;
  }

  std::unique_lock lock(callbackMutex_);
  callbacks_.emplace_back(ev, ev_handler);
}

void IPC::unregisterForIPC(EventHandler *ev_handler) {
  if (ev_handler == nullptr) {
    return;
  }

  std::unique_lock lock(callbackMutex_);

  for (auto it = callbacks_.begin(); it != callbacks_.end();) {
    auto &[eventname, handler] = *it;
    if (handler == ev_handler) {
      it = callbacks_.erase(it);
    } else {
      ++it;
    }
  }
}

Json::Value IPC::send(const Json::Value &request) {
  int socketfd = connectToSocket();
  if (socketfd == -1) throw std::runtime_error("Niri is not running");

  auto unix_istream = Gio::UnixInputStream::create(socketfd, true);
  auto unix_ostream = Gio::UnixOutputStream::create(socketfd, false);
  auto istream = Gio::DataInputStream::create(unix_istream);
  auto ostream = Gio::DataOutputStream::create(unix_ostream);

  // Niri needs the request on a single line.
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
  std::ostringstream oss;
  writer->write(request, &oss);
  oss << '\n';

  if (!ostream->put_string(oss.str()) || !ostream->flush())
    throw std::runtime_error("error writing to niri socket");

  std::string line;
  if (!istream->read_line(line)) throw std::runtime_error("error reading from niri socket");

  std::istringstream iss(std::move(line));
  Json::Value response;
  iss >> response;
  return response;
}

}  // namespace waybar::modules::niri
