#pragma once

#include <gtkmm.h>
#include <wayland-client.h>

namespace waybar {

class IModule {
  public:
    virtual ~IModule() = default;
    virtual auto update() -> void = 0;
    virtual operator Gtk::Widget &() = 0;
  virtual void handleSeat(struct wl_seat*, uint32_t) {};
    Glib::Dispatcher dp; // Hmmm Maybe I should create an abstract class ?
};

}
