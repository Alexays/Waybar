#pragma once
#include <list>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace waybar::modules::hyprland {

class EventHandler {
public:
  virtual void onEvent(const std::string& ev) = 0;
  virtual ~EventHandler() = default;
};

class IPC {
 public:
  IPC() { startIPC(); }

  void registerForIPC(const std::string&, EventHandler*);
  void unregisterForIPC(EventHandler*);

  std::string getSocket1Reply(const std::string& rq);

 private:
  void startIPC();
  void parseIPC(const std::string&);

  std::mutex callbackMutex;
  std::list<std::pair<std::string, EventHandler*>> callbacks;
};

inline std::unique_ptr<IPC> gIPC;
inline bool modulesReady = false;
};  // namespace waybar::modules::hyprland
