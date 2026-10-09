// include/modules/mango/backend.hpp
#pragma once

#include <atomic>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "util/json.hpp"

namespace waybar::modules::mango {

class EventHandler {
 public:
  virtual void onEvent(const Json::Value& ev) = 0;
  virtual ~EventHandler() = default;
};

class IPC {
 public:
  static IPC& getInstance();
  IPC(const IPC&) = delete;
  IPC& operator=(const IPC&) = delete;

  void registerForIPC(const std::string& ev, EventHandler* handler);
  void unregisterForIPC(EventHandler* handler);

  static Json::Value send(const Json::Value& request);
  static void sendAsync(const Json::Value& request);

  std::unique_lock<std::mutex> lockData() { return std::unique_lock<std::mutex>(data_mutex_); }

  std::unordered_map<std::string, Json::Value> getMonitors() const;
  Json::Value getMonitor(const std::string& name);
  Json::Value getActiveClientForMonitor(const std::string& name) const;
  std::string getKeyboardLayout() const;
  std::string getKeymode() const;
  std::string getLayoutSymbolForMonitor(const std::string& name) const;

 private:
  IPC();
  ~IPC();
  void startIPC();
  static int connectToSocket();
  void parseIPC(const std::string& line);

  void handleMonitorUpdate(const Json::Value& mon);
  void updateFocusingClient(const Json::Value& client);
  void updateKeyboardLayout(const std::string& layout);

  static Json::Value sendCommand(const std::string& cmd);

  std::atomic<bool> running_ = true;
  int sockfd_ = -1;
  std::thread ipc_thread_;
  mutable std::mutex data_mutex_;
  std::unordered_map<std::string, Json::Value> monitors_;
  std::unordered_map<uint64_t, Json::Value> clients_;
  uint64_t focusing_client_id_ = 0;
  std::string keyboard_layout_;
  std::string keymode_;
  Json::Value active_client_;
  std::mutex callback_mutex_;
  std::list<std::pair<std::string, EventHandler*>> callbacks_;
};
}  // namespace waybar::modules::mango