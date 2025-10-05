#pragma once

#include <filesystem>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "util/json.hpp"

namespace waybar::modules::hyprland {

class EventHandler {
 public:
  virtual void onEvent(const std::string& ev) = 0;
  virtual ~EventHandler() = default;
};

/// If you want to use the Hyprland IPC, simply use IPC::inst() to get the singleton instance.
/// Do not create multiple instances.
class IPC {
 protected:
  IPC();  // use IPC::inst() instead.

 public:
  ~IPC();
  static IPC& inst();

  void registerForIPC(const std::string& ev, EventHandler* ev_handler);
  void unregisterForIPC(EventHandler* handler);

  static std::string getSocket1Reply(const std::string& rq);
  Json::Value getSocket1JsonReply(const std::string& rq);
  static std::filesystem::path getSocketFolder(const char* instanceSig);

 protected:
  static std::filesystem::path socketFolder_;

 private:
  void socketListener();
  void parseIPC(const std::string&);

  std::thread ipcThread_;
  std::mutex callbackMutex_;
  util::JsonParser parser_;
  std::list<std::pair<std::string, EventHandler*>> callbacks_;
  int socketfd_;         // the hyprland socket file descriptor
  pid_t socketOwnerPid_;
  bool running_ = true;  // the ipcThread will stop running when this is false
};
};  // namespace waybar::modules::hyprland
