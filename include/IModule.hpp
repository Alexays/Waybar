#pragma once

#include <gtkmm.h>

namespace waybar {

class IModule {
  public:
    virtual ~IModule() = default;
    virtual auto update() -> void = 0;
    virtual operator Gtk::Widget &() = 0;
    Glib::Dispatcher dp; // Hmmm Maybe I should create an abstract class ?
};

}
