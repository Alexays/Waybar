#pragma once

#include <gtkmm/widget.h>

namespace waybar {

class IWidget {
 public:
  virtual ~IWidget() = default;
  virtual auto update() -> void = 0;
  virtual operator Gtk::Widget&() = 0;
};

}  // namespace waybar
