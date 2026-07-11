#pragma once

#include <atomic>
#include <filesystem>
#include <list>
#include <mutex>
#include <optional>
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

  /// Dispatch a Hyprland command. Automatically uses the correct protocol
  /// (legacy text or Lua-based) depending on the running Hyprland version.
  static std::string dispatch(const std::string& dispatcher, const std::string& arg);

  /// Build a Lua-format dispatch command string.
  static std::string buildLuaDispatch(const std::string& dispatcher, const std::string& arg);

 protected:
  static std::filesystem::path socketFolder_;

  /// Detect whether the running Hyprland uses the Lua-based IPC protocol.
  /// Returns true for Hyprland >= 0.54 (Lua config), false for older versions.
  static bool isLuaProtocol();

  static std::optional<bool> s_luaProtocolDetected_;  // cached detection result

 private:
  void socketListener();
  void parseIPC(const std::string&);

  std::thread ipcThread_;
  std::mutex callbackMutex_;
  std::mutex socketMutex_;
  util::JsonParser parser_;
  std::list<std::pair<std::string, EventHandler*>> callbacks_;
  int socketfd_ = -1;  // the hyprland socket file descriptor
  pid_t socketOwnerPid_ = -1;
  std::atomic<bool> running_ = true;  // the ipcThread will stop running when this is false
};
};  // namespace waybar::modules::hyprland
