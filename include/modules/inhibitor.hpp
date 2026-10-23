#pragma once

#include <gio/gio.h>

#include <memory>

#include "ALabel.hpp"
#include "bar.hpp"

namespace waybar::modules {

class Inhibitor final : public ALabel {
 public:
  Inhibitor(const std::string&, const waybar::Bar&, const Json::Value&);
  virtual ~Inhibitor();
  auto doUpdate() -> void override;
  auto activated() -> bool;

 private:
  void handlePress(int n_press, double x, double y) override;

  const std::unique_ptr<::GDBusConnection, void (*)(::GDBusConnection*)> dbus_;
  const std::string inhibitors_;
  int handle_ = -1;
};

}  // namespace waybar::modules
