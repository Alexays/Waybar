#pragma once

#include <gtkmm/widget.h>

namespace waybar {

class IModule {
 public:
  virtual ~IModule() = default;
  virtual void doUpdate() = 0;
  virtual void doAction(const std::string& name) = 0;
  virtual void doSuspend() = 0;
  virtual void doResume() = 0;
  virtual operator Gtk::Widget&() = 0;
};

}  // namespace waybar
