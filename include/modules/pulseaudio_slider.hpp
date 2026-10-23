#pragma once

#include <memory>

#include "ASlider.hpp"
#include "util/audio_backend.hpp"

namespace waybar::modules {

class PulseaudioSlider final : public ASlider {
 public:
  PulseaudioSlider(const std::string&, const Json::Value&);
  virtual ~PulseaudioSlider() = default;
  void doUpdate() override;

 private:
  void onValueChanged() override;

  std::shared_ptr<util::AudioBackend> backend = nullptr;
  util::PulseaudioTarget target = util::PulseaudioTarget::Sink;

  bool zero_on_mute = true;
  bool unmute_on_volume_change = true;
  // zero_on_mute and unmute_on_volume_change default to true
  // in order to maintain previous behaviour when using a
  // config in which these values are undefined

  bool previously_muted = false;
};

}  // namespace waybar::modules
