#pragma once

#include <gtkmm/box.h>
#include <gtkmm/widget.h>
#include <json/json.h>

#include "AModule.hpp"
#include "bar.hpp"
#include "factory.hpp"

namespace waybar {

class Group : public AModule {
 public:
  Group(const std::string&, const std::string&, const Json::Value&, bool);
  ~Group() = default;
  auto update() -> void override;
  operator Gtk::Widget&() override;
  Gtk::Box box;
};

}  // namespace waybar
