#pragma once

#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "util/json.hpp"

namespace waybar::modules::umbriel {

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
  void unregisterForIPC(EventHandler* ev_handler);

  // Sends one request to Umbriel and returns its reply. Throws when Umbriel
  // answers with an error, so callers can log the compositor's message.
  static Json::Value send(const Json::Value& request);

  // The data members are only safe to access while dataMutex_ is locked.
  std::lock_guard<std::mutex> lockData() { return std::lock_guard(dataMutex_); }
  const std::vector<Json::Value>& workspaces() const { return workspaces_; }
  const std::vector<Json::Value>& windows() const { return windows_; }

 private:
  static std::string socketPath();
  static int connectToSocket();
  static std::string serialize(const Json::Value& request);

  void startIPC(int initial_socketfd);
  void parseIPC(const std::string& line);

  std::mutex dataMutex_;
  std::vector<Json::Value> workspaces_;
  std::vector<Json::Value> windows_;

  util::JsonParser parser_;
  std::mutex callbackMutex_;
  std::list<std::pair<std::string, EventHandler*>> callbacks_;

  std::atomic<bool> running_{true};
};

inline std::unique_ptr<IPC> gIPC;

}  // namespace waybar::modules::umbriel
