#pragma once

#include "ALabel.hpp"

namespace waybar::modules {

class Inhibitor final : public ALabel {
 public:
  Inhibitor(const std::string&, const Json::Value&);
  virtual ~Inhibitor();
  auto update() -> void override;
  auto doAction(const std::string& name) -> void override;
  auto activated() -> bool;

 private:
  const std::unique_ptr<::GDBusConnection, void (*)(::GDBusConnection*)> dbus_;
  const std::string inhibitors_;
  int handle_ = -1;
  // Module actions
  void toggle();
  // Module Action Map
  static inline std::map<const std::string, void (waybar::modules::Inhibitor::*const)()> actionMap_{
      {"toggle", &waybar::modules::Inhibitor::toggle}};
};

}  // namespace waybar::modules
