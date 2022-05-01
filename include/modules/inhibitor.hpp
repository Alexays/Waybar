#pragma once

#include <gio/gio.h>

#include <memory>

#include "ALabel.hpp"
#include "bar.hpp"

namespace waybar::modules {

class Inhibitor : public ALabel {
 public:
  Inhibitor(const std::string&, const waybar::Bar&, const Json::Value&);
  ~Inhibitor() override;
  auto update() -> void;
  auto activated() -> bool;

 private:
  auto handleToggle(::GdkEventButton* const& e) -> bool;

  const std::unique_ptr<::GDBusConnection, void (*)(::GDBusConnection*)> dbus_;
  const std::string inhibitors_;
  int handle_ = -1;
};

}  // namespace waybar::modules
