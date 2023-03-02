#pragma once

#include <gtkmm/label.h>

#include <mutex>
#include <string>

#include "ALabel.hpp"
#include "bar.hpp"
#include "client.hpp"
#include "modules/sway/ipc/client.hpp"
#include "util/json.hpp"

namespace waybar::modules::sway {
class Scratchpad : public ALabel {
 public:
  Scratchpad(const std::string&, const Json::Value&);
  virtual ~Scratchpad() = default;
  auto update() -> void override;

 private:
  auto getTree() -> void;
  auto onCmd(const struct Ipc::ipc_response&) -> void;
  auto onEvent(const struct Ipc::ipc_response&) -> void;

  std::string tooltip_format_;
  bool show_empty_;
  bool tooltip_enabled_;
  std::string tooltip_text_;
  int count_;
  std::mutex mutex_;
  Ipc ipc_;
  util::JsonParser parser_;
};
}  // namespace waybar::modules::sway
