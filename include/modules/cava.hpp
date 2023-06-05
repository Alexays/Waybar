#pragma once

#include "ALabel.hpp"
#include "modules/DBusClient.hpp"

namespace waybar::modules {

class Cava final: public ALabel, public DBusClient {
 public:
  Cava(const std::string &, const Json::Value&);
  virtual ~Cava() = default;
  auto resultCallback() -> void override;
  auto update() -> void override;
  auto doAction(const Glib::ustring &name = "") -> void override;
  resultMap* const getResultMap() override;
 private:
  util::SleeperThread thread_;
  std::chrono::milliseconds frame_time_milsec_{(int)(1e3 / 30)};
  Glib::ustring resFrameTime;

  // Result map
  std::map<const std::string, Glib::ustring*> resultMap_ {
    {"getLabelText", &resLabel},
    {"getTooltipText", &resTooltip},
    {"getFrameTime_mil", &resFrameTime}};
};
}
