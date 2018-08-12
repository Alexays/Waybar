#pragma once

#include <gtkmm.h>

namespace waybar {
  class IModule {
	  public:
      virtual ~IModule() {}
      virtual auto update() -> void = 0;
      virtual operator Gtk::Widget &() = 0;
  };
}
