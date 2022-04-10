#include "group.hpp"

#include <fmt/format.h>

#include <util/command.hpp>

namespace waybar {

Group::Group(const std::string& name, const Bar& bar, const Json::Value& config)
    : AModule(config, name, "", false, false),
      box{bar.vertical ? Gtk::ORIENTATION_HORIZONTAL : Gtk::ORIENTATION_VERTICAL, 0} {}

auto Group::update() -> void {
  // noop
}

Group::operator Gtk::Widget&() { return box; }

}  // namespace waybar
