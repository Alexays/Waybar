#pragma once

#include <fmt/format.h>

#include <tuple>

#include "AIconLabel.hpp"
#include "bar.hpp"
#include "client.hpp"
#include "modules/sway/ipc/client.hpp"
#include "util/json.hpp"

namespace waybar::modules::sway {

class Window : public AIconLabel, public sigc::trackable {
 public:
  Window(const std::string&, const waybar::Bar&, const Json::Value&);
  ~Window() = default;
  auto update() -> void;

 private:
  void onEvent(const struct Ipc::ipc_response&);
  void onCmd(const struct Ipc::ipc_response&);
  std::tuple<std::size_t, int, std::string, std::string, std::string, std::string> getFocusedNode(
      const Json::Value& nodes, std::string& output);
  void getTree();
  void updateAppIconName();
  void updateAppIcon();

  const Bar& bar_;
  std::string window_;
  int windowId_;
  std::string app_id_;
  std::string app_class_;
  std::string old_app_id_;
  std::size_t app_nb_;
  std::string shell_;
  unsigned app_icon_size_{24};
  bool update_app_icon_{true};
  std::string app_icon_name_;
  util::JsonParser parser_;
  std::mutex mutex_;
  Ipc ipc_;
};

}  // namespace waybar::modules::sway
