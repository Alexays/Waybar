#pragma once

#include <tuple>

#include "ALabel.hpp"
#include "sway/ipc/client.hpp"
#include "util/json.hpp"
#include "util/sleeper_thread.hpp"
#include "wnd/display/display.hpp"

namespace waybar::modules {
class Wnd : public ALabel {
 public:
  Wnd(const std::string &, const Json::Value &);
  ~Wnd() = default;
  auto update() -> void;

  void onEvent(const struct sway::Ipc::ipc_response &);
  void onCmd(const struct sway::Ipc::ipc_response &);
  void getTree();
  std::tuple<std::size_t, int, std::string> getFocusedNode(const Json::Value &nodes);

 private:
  std::string window_;
  int windowId_;
  std::string app_id_;
  std::string app_class_;
  std::string old_app_id_;
  std::size_t app_nb_;
  std::string shell_;
  util::JsonParser parser_;
  util::SleeperThread thread_;
  wnd::display::Display display_;
  std::mutex mutex_;
  sway::Ipc ipc_;
};
};  // namespace waybar::modules
