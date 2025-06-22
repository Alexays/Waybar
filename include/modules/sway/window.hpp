#pragma once

#include <fmt/format.h>

#include <tuple>

#include "AAppIconLabel.hpp"
#include "bar.hpp"
#include "client.hpp"
#include "modules/sway/ipc/client.hpp"
#include "util/json.hpp"

namespace waybar::modules::sway {

class Window : public AAppIconLabel, public sigc::trackable {
 public:
  Window(const std::string&, const waybar::Bar&, const Json::Value&);
  virtual ~Window() = default;
  auto update() -> void override;

 private:
  void setClass(std::string classname, bool enable);
  void onEvent(const struct Ipc::ipc_response&);
  void onCmd(const struct Ipc::ipc_response&);
  std::tuple<std::size_t, int, int, std::string, std::string, std::string, std::string, std::string>
  getFocusedNode(const Json::Value& nodes, std::string& output);
  void getTree();

  const Bar& bar_;
  std::string window_;
  int windowId_;
  std::string app_id_;
  std::string app_class_;
  std::string layout_;
  std::string old_app_id_;
  std::size_t app_nb_;
  std::string shell_;
  int floating_count_;
  util::JsonParser parser_;
  std::mutex mutex_;
  Ipc ipc_;
};

}  // namespace waybar::modules::sway
