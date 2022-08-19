#pragma once
#include <string>
#include <memory>
#include <mutex>
#include <deque>
#include <functional>
#include <thread>

namespace waybar::modules::hyprland {
class IPC {
public:
  IPC() { startIPC(); }

  void registerForIPC(const std::string&, std::function<void(const std::string&)>);

  std::string getSocket1Reply(const std::string& rq);

private:

 void startIPC();
 void parseIPC(const std::string&);

 std::mutex callbackMutex;
 std::deque<std::pair<std::string, std::function<void(const std::string&)>>> callbacks;
};

inline std::unique_ptr<IPC> gIPC;
inline bool modulesReady = false;
};

