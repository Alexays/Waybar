#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <json/json.h>
#include <sigc++/sigc++.h>

#include "util/SafeSignal.hpp"
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

class CavaBackend final {
 public:
  static std::shared_ptr<CavaBackend> inst(const Json::Value& config);

  virtual ~CavaBackend();
  // Methods
  int getAsciiRange() const;
  void doPauseResume();
  void update();
  const ::cava::config_params& getPrm() const;
  std::chrono::milliseconds getFrameTimeMilsec() const;

  struct AudioRaw {
    std::vector<float> bars_raw;
    std::vector<float> previous_bars_raw;
    int number_of_bars = 0;

    AudioRaw() = default;
    explicit AudioRaw(const ::cava::audio_raw& raw) {
      number_of_bars = raw.number_of_bars;
      if (raw.bars_raw != nullptr && number_of_bars > 0) {
        bars_raw.assign(raw.bars_raw, raw.bars_raw + number_of_bars);
      }
      if (raw.previous_bars_raw != nullptr && number_of_bars > 0) {
        previous_bars_raw.assign(raw.previous_bars_raw, raw.previous_bars_raw + number_of_bars);
      }
    }
  };

  // Signal accessor
  using SignalUpdate = SafeSignal<const std::string&>;
  SignalUpdate& signalUpdate();
  using SignalAudioRawUpdate = SafeSignal<AudioRaw>;
  SignalAudioRawUpdate& signalAudioRawUpdate();
  using SignalSilence = SafeSignal<>;
  SignalSilence& signalSilence();
  using SignalConfigChanged = SafeSignal<>;
  SignalConfigChanged& signalConfigChanged();

 private:
  CavaBackend(const Json::Value& config);
  util::SleeperThread read_thread_;
  util::SleeperThread out_thread_;

  // Cava API to read audio source
  ::cava::ptr input_source_{nullptr};

  struct ::cava::error_s error_{};          // cava errors
  struct ::cava::config_params prm_{};      // cava parameters
  struct ::cava::audio_raw audio_raw_{};    // cava handled raw audio data(is based on audio_data)
  struct ::cava::audio_data audio_data_{};  // cava audio data
  struct ::cava::cava_plan* plan_{nullptr};    //{new cava_plan{}};

  std::chrono::seconds fetch_input_delay_{4};

  struct AdaptiveDelay {
    std::chrono::milliseconds delay;
    std::chrono::seconds delta{0};

    explicit AdaptiveDelay(std::chrono::milliseconds initial = std::chrono::seconds(1))
        : delay(initial) {}

    bool increase() {
      if (delta == std::chrono::seconds{0}) {
        delta += std::chrono::seconds{1};
        delay += delta;
        return true;
      }
      return false;
    }

    bool decrease() {
      if (delta > std::chrono::seconds{0}) {
        delay -= delta;
        delta -= std::chrono::seconds{1};
        return true;
      }
      return false;
    }

    std::chrono::milliseconds current() const { return delay; }

    void reset(std::chrono::milliseconds new_delay) {
      delay = new_delay;
      delta = std::chrono::seconds{0};
    }
  };

  AdaptiveDelay adaptive_delay_;

  Json::Value config_;
  int re_paint_{0};
  bool silence_{false};
  bool silence_prev_{false};
  int sleep_counter_{0};
  std::string output_{};
  // Methods
  void invoke();
  void execute();
  bool isSilent();
  void doUpdate(bool force = false);
  void loadConfig();
  void freeBackend();

  // Signal
  SignalUpdate m_signal_update_;
  SignalAudioRawUpdate m_signal_audio_raw_;
  SignalSilence m_signal_silence_;
  SignalConfigChanged m_signal_config_changed_;

  std::atomic<bool> shutdown_{false};
  bool audio_raw_initialized_{false};
  mutable std::recursive_mutex state_mutex_;

  // Synchronization for joining read_thread_ during destruction
  bool read_thread_exited_{false};
  mutable std::mutex read_thread_exit_mutex_;
  std::condition_variable read_thread_exit_cv_;
};
}  // namespace waybar::modules::cava
