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
// class Scratchpad : public AModule, public sigc::trackable {
class Scratchpad : public ALabel {
 public:
  Scratchpad(const std::string&, const waybar::Bar&, const Json::Value&);
  ~Scratchpad() = default;
  auto update() -> void;

 private:
  auto getTree() -> void;
  auto onCmd(const struct Ipc::ipc_response&) -> void;
  auto onEvent(const struct Ipc::ipc_response&) -> void;
  // bool handleScroll(GdkEventScroll*);
  Gtk::Box box_;
  const Bar& bar_;
  std::mutex mutex_;
  int count_;
  Ipc ipc_;
  util::JsonParser parser_;
};
}  // namespace waybar::modules::sway