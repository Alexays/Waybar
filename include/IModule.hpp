#pragma once

#include <gtkmm/widget.h>

#include <sys/types.h>
#include <list>
#include <mutex>

namespace waybar {

class IModule {
 public:
  virtual ~IModule() = default;
  virtual auto update() -> void = 0;
  virtual operator Gtk::Widget&() = 0;
  virtual auto doAction(const std::string& name) -> void = 0;
};

}  // namespace waybar
