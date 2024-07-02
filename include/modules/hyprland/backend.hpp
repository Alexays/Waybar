#pragma once

#include <filesystem>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "util/json.hpp"

namespace waybar::modules::hyprland {

class EventHandler {
 public:
  virtual void onEvent(const std::string& ev) = 0;
  virtual ~EventHandler() = default;
};

class IPC {
 public:
  IPC() { startIPC(); }

  void registerForIPC(const std::string& ev, EventHandler* ev_handler);
  void unregisterForIPC(EventHandler* handler);

  static std::string getSocket1Reply(const std::string& rq);
  Json::Value getSocket1JsonReply(const std::string& rq);
  static std::filesystem::path getSocketFolder(const char* instanceSig);

 protected:
  static std::filesystem::path socketFolder_;

 private:
  void startIPC();
  void parseIPC(const std::string&);

  std::mutex callbackMutex_;
  util::JsonParser parser_;
  std::list<std::pair<std::string, EventHandler*>> callbacks_;
};

inline std::unique_ptr<IPC> gIPC;
inline bool modulesReady = false;
};  // namespace waybar::modules::hyprland
