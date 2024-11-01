#pragma once

#include <gtkmm/widget.h>

namespace waybar {

class IModule {
 public:
  virtual ~IModule() = default;
  virtual auto update() -> void = 0;
  // Get the root widget of the module
  virtual Gtk::Widget& root() = 0;
  virtual auto doAction(const std::string& name) -> void = 0;

  operator Gtk::Widget&() { return this->root(); }
};

}  // namespace waybar
