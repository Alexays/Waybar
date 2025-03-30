#pragma once

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace cava {
extern "C" {
#include <cava/common.h>
}
}  // namespace cava

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

  struct cava::error_s error_{};          // cava errors
  struct cava::config_params prm_{};      // cava parameters
  struct cava::audio_raw audio_raw_{};    // cava handled raw audio data(is based on audio_data)
  struct cava::audio_data audio_data_{};  // cava audio data
  struct cava::cava_plan* plan_;          //{new cava_plan{}};
  // Cava API to read audio source
  cava::ptr input_source_;
  // Delay to handle audio source
  std::chrono::milliseconds frame_time_milsec_{1s};
  // Text to display
  std::string text_{""};
  int rePaint_{1};
  std::chrono::seconds fetch_input_delay_{4};
  std::chrono::seconds suspend_silence_delay_{0};
  bool silence_{false};
  bool hide_on_silence_{false};
  std::string format_silent_{""};
  int sleep_counter_{0};
  // Cava method
  void pause_resume();
  // ModuleActionMap
  static inline std::map<const std::string, void (waybar::modules::Cava::* const)()> actionMap_{
      {"mode", &waybar::modules::Cava::pause_resume}};
};
}  // namespace waybar::modules
