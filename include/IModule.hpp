#pragma once

#include <glibmm/dispatcher.h>
#include <gtkmm/box.h>
#include <gtkmm/widget.h>

namespace waybar {

class IModule {
 public:
  virtual ~IModule() = default;
  virtual auto     update() -> void = 0;
  virtual          operator Gtk::Widget &() = 0;
  Glib::Dispatcher dp;  // Hmmm Maybe I should create an abstract class ?
};

}  // namespace waybar
