#pragma once

#include <string>
#include <glibmm/ustring.h>

namespace waybar {

class IModule {
 public:
  virtual ~IModule() = default;
  virtual auto doAction(const Glib::ustring &name = "") -> void = 0;
};

}  // namespace waybar
