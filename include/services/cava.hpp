#pragma once

#include "services/DBusService.hpp"
#include "util/date.hpp"

extern "C" {
#include <cava/common.h>
}

namespace waybar::services {

using namespace std::literals::chrono_literals;

class Cava final : public DBusService {
 public:
  Cava(const std::string&, const Json::Value&);
  virtual ~Cava();
  auto doAction(const Glib::ustring &name) -> void override;
  bool doActionExists(const Glib::ustring &name) override;
  const Glib::ustring& doMethod(const Glib::ustring &name) override;
  const bool doMethodExists(const Glib::ustring &name);
  auto start() -> void override;
  auto stop() -> void override;
 private:
  Glib::ustring textFrameTime_{""};
  util::SleeperThread thread_;

  struct error_s error_ {};          // cava errors
  struct config_params prm_ {};      // cava parameters
  struct audio_raw audio_raw_ {};    // cava handled raw audio data(is based on audio_data)
  struct audio_data audio_data_ {};  // cava audio data
  struct cava_plan* plan_;           //{new cava_plan{}};
  // Cava API to read audio source
  ptr input_source_;
  // Delay to handle audio source
  std::chrono::milliseconds frame_time_milsec_{1s};
  int rePaint_{1};
  std::chrono::seconds fetch_input_delay_{4};
  std::chrono::seconds suspend_silence_delay_{0};
  bool silence_{false};
  int sleep_counter_{0};
  // Cava method
  void pause_resume();
  const Glib::ustring& getFrameTime();
  const Glib::ustring& getLabelText() override;

  // ModuleActionMap
  static inline std::map<const std::string, void (waybar::services::Cava::*const)()> actionMap_{
      {"mode", &waybar::services::Cava::pause_resume}};

  static inline std::map<const std::string, const Glib::ustring&(waybar::services::Cava::*const)()> methodMap_ {
    {"getLabelText", &waybar::services::Cava::getLabelText},
    {"getTooltipText", &waybar::services::Cava::getTooltipText},
    {"getFrameTime_mil", &waybar::services::Cava::getFrameTime}};
};

} // namespace waybar::service
