#pragma once

#include <mutex>

#include "ALabel.hpp"
#include "modules/sway/ipc/client.hpp"
#include "util/json.hpp"

namespace waybar::modules::sway {
class Scratchpad final : public ALabel {
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
