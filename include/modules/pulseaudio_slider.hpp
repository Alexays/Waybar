#pragma once

#include <memory>

#include "ASlider.hpp"
#include "util/audio_backend.hpp"
namespace waybar::modules {

class PulseaudioSlider : public ASlider {
 public:
  PulseaudioSlider(const std::string&, const Json::Value&);
  virtual ~PulseaudioSlider() = default;

  void update() override;
  void onValueChanged() override;

 private:
  std::shared_ptr<util::AudioBackend> backend = nullptr;
  util::AudioTarget target = util::AudioTarget::Sink;
};

}  // namespace waybar::modules