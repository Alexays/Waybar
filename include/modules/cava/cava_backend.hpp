#pragma once

#include <json/json.h>
#include <sigc++/sigc++.h>

#include "util/sleeper_thread.hpp"

namespace cava {
extern "C" {
// Need sdl_glsl output feature to be enabled on libcava
#ifndef SDL_GLSL
#define SDL_GLSL
#endif

#include <cava/common.h>

#ifdef SDL_GLSL
#undef SDL_GLSL
#endif
}
}  // namespace cava

namespace waybar::modules::cava {
using namespace std::literals::chrono_literals;

class CavaBackend final {
 public:
  static std::shared_ptr<CavaBackend> inst(const Json::Value& config);

  virtual ~CavaBackend();
  // Methods
  int getAsciiRange();
  void doPauseResume();
  void Update();
  // Signal accessor
  using type_signal_update = sigc::signal<void(const std::string&)>;
  type_signal_update signal_update();
  using type_signal_silence = sigc::signal<void()>;
  type_signal_silence signal_silence();

 private:
  CavaBackend(const Json::Value& config);
  util::SleeperThread read_thread_;
  sigc::connection out_thread_;
  // Cava API to read audio source
  ::cava::ptr input_source_{NULL};

  struct ::cava::error_s error_{};          // cava errors
  struct ::cava::config_params prm_{};      // cava parameters
  struct ::cava::audio_raw audio_raw_{};    // cava handled raw audio data(is based on audio_data)
  struct ::cava::audio_data audio_data_{};  // cava audio data
  struct ::cava::cava_plan* plan_{NULL};    //{new cava_plan{}};

  std::chrono::seconds fetch_input_delay_{4};
  // Delay to handle audio source
  std::chrono::milliseconds frame_time_milsec_{1s};

  const Json::Value& config_;
  int re_paint_{0};
  bool silence_{false};
  bool silence_prev_{false};
  std::chrono::seconds suspend_silence_delay_{0};
  int sleep_counter_{0};
  std::string output_{};
  // Methods
  void invoke();
  void execute();
  bool isSilence();
  void doUpdate(bool force = false);
  void loadConfig();
  void freeBackend();
  void doOutReadConnect();

  // Signal
  type_signal_update m_signal_update_;
  type_signal_silence m_signal_silence_;
};
}  // namespace waybar::modules::cava
