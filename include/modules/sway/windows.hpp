#pragma once

#include <fmt/format.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>

#include <unordered_map>

#include "AModule.hpp"
#include "bar.hpp"
#include "client.hpp"
#include "modules/sway/ipc/client.hpp"
#include "util/json.hpp"

namespace waybar::modules::sway {

class Windows : public AModule, public sigc::trackable {
 public:
  Windows(const std::string&, const waybar::Bar&, const Json::Value&);
  ~Windows() = default;
  auto update() -> void;

 private:
  static inline const std::string window_switch_cmd_ = "[con_id={}] focus {}";

  void onCmd(const struct Ipc::ipc_response&);
  void onEvent(const struct Ipc::ipc_response&);
  bool filterButtons();
  Gtk::Button& addButton(const Json::Value&);
  void onButtonReady(const Json::Value&, Gtk::Button&);
  std::string getIcon(const std::string&, const Json::Value&);
  const int getCycleWindow(std::vector<Json::Value>::iterator, bool prev) const;
  std::string trimWindowName(std::string);
  bool handleScroll(GdkEventScroll*);

  const Bar& bar_;
  Json::Value workspace_;
  std::vector<Json::Value> windows_;
  Gtk::Box box_;
  util::JsonParser parser_;
  std::unordered_map<int, Gtk::Button> buttons_;
  std::mutex mutex_;
  Ipc ipc_;
};

}  // namespace waybar::modules::sway
