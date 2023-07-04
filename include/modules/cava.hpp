#pragma once

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

extern "C" {
#include <cava/common.h>
}

namespace waybar::modules {
using namespace std::literals::chrono_literals;

class Cava final : public ALabel {
 public:
  Cava(const std::string&, const Json::Value&);
  virtual ~Cava();
  auto update() -> void override;
  auto doAction(const std::string& name) -> void override;

 private:
  util::SleeperThread thread_;
  util::SleeperThread thread_fetch_input_;

  struct error_s error_ {};          // cava errors
  struct config_params prm_ {};      // cava parameters
  struct audio_raw audio_raw_ {};    // cava handled raw audio data(is based on audio_data)
  struct audio_data audio_data_ {};  // cava audio data
  struct cava_plan* plan_;           //{new cava_plan{}};
  // Cava API to read audio source
  ptr input_source_;
  // Delay to handle audio source
  std::chrono::milliseconds frame_time_milsec_{1s};
  // Text to display
  std::string text_{""};
  int rePaint_{1};
  std::chrono::seconds fetch_input_delay_{4};
  std::chrono::seconds suspend_silence_delay_{0};
  bool silence_{false};
  int sleep_counter_{0};
  // Cava method
  void pause_resume();
  // ModuleActionMap
  static inline std::map<const std::string, void (waybar::modules::Cava::*const)()> actionMap_{
      {"mode", &waybar::modules::Cava::pause_resume}};
};
}  // namespace waybar::modules
