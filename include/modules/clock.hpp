#pragma once

#include "ALabel.hpp"
#include "modules/DBusClient.hpp"

namespace waybar::modules {

class Clock final : public ALabel, public DBusClient {
 public:
  Clock(const std::string&, const Json::Value&);
  virtual ~Clock() = default;
  auto resultCallback() -> void override;
  auto update() -> void override;
  auto doAction(const Glib::ustring &name = "") -> void override;
 private:
  util::SleeperThread thread_;
};

}
