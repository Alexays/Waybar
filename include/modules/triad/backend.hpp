#pragma once

#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "util/json.hpp"

namespace waybar::modules::triad {

class EventHandler {
 public:
  virtual void onEvent(const Json::Value& ev) = 0;
  virtual ~EventHandler() = default;
};

class IPC {
 public:
  IPC();

  void registerForIPC(const std::string& ev, EventHandler* ev_handler);
  void unregisterForIPC(EventHandler* handler);

  static Json::Value send(const Json::Value& request);
  static Json::Value action(const std::string& action_name);
  static Json::Value action(const std::string& action_name, const std::string& key,
                            const Json::Value& value);

  std::lock_guard<std::mutex> lockData() { return std::lock_guard(dataMutex_); }
  const std::vector<Json::Value>& workspaces() const { return workspaces_; }
  const std::vector<Json::Value>& windows() const { return windows_; }
  const std::vector<Json::Value>& outputs() const { return outputs_; }
  const std::vector<std::string>& keyboardLayoutNames() const { return keyboardLayoutNames_; }
  unsigned keyboardLayoutCurrent() const { return keyboardLayoutCurrent_; }

 private:
  void startIPC();
  static int connectToSocket();
  static Json::Value request(const std::string& name);
  void loadState();
  void parseIPC(const std::string&);
  void parseState(const Json::Value& state);
  void parseLayoutState(const Json::Value& state);
  void parseWindowChanged(const Json::Value& window);

  std::mutex dataMutex_;
  std::vector<Json::Value> workspaces_;
  std::vector<Json::Value> windows_;
  std::vector<Json::Value> outputs_;
  std::vector<std::string> keyboardLayoutNames_;
  unsigned keyboardLayoutCurrent_ = 0;

  util::JsonParser parser_;
  std::mutex callbackMutex_;
  std::list<std::pair<std::string, EventHandler*>> callbacks_;
};

inline std::unique_ptr<IPC> gIPC;

}  // namespace waybar::modules::triad
