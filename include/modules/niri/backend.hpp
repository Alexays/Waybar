#pragma once

#include <atomic>
#include <list>
#include <mutex>
#include <string>
#include <utility>

#include "util/json.hpp"

namespace waybar::modules::niri {

class EventHandler {
 public:
  virtual void onEvent(const Json::Value& ev) = 0;
  virtual ~EventHandler() = default;
};

class IPC {
 public:
  IPC();
  ~IPC();

  void registerForIPC(const std::string& ev, EventHandler* ev_handler);
  void unregisterForIPC(EventHandler* handler);

  static Json::Value send(const Json::Value& request);

  // The data members are only safe to access while dataMutex_ is locked.
  std::lock_guard<std::mutex> lockData() { return std::lock_guard(dataMutex_); }
  const std::vector<Json::Value>& workspaces() const { return workspaces_; }
  const std::vector<Json::Value>& windows() const { return windows_; }
  const std::vector<std::string>& keyboardLayoutNames() const { return keyboardLayoutNames_; }
  unsigned keyboardLayoutCurrent() const { return keyboardLayoutCurrent_; }

 private:
  void startIPC(int initial_socketfd);
  static int connectToSocket();
  void parseIPC(const std::string&);

  std::mutex dataMutex_;
  std::vector<Json::Value> workspaces_;
  std::vector<Json::Value> windows_;
  std::vector<std::string> keyboardLayoutNames_;
  unsigned keyboardLayoutCurrent_;

  util::JsonParser parser_;
  std::mutex callbackMutex_;
  std::list<std::pair<std::string, EventHandler*>> callbacks_;

  std::atomic<bool> running_{true};
};

inline std::unique_ptr<IPC> gIPC;

};  // namespace waybar::modules::niri
