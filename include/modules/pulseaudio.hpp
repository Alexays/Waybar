#pragma once

#include <algorithm>
#include <array>
#include <memory>

#include "ALabel.hpp"
#include "util/audio_backend.hpp"

namespace waybar::modules {

class Pulseaudio final : public ALabel {
 public:
  Pulseaudio(const std::string&, const Json::Value&);
  virtual ~Pulseaudio() = default;
  auto doUpdate() -> void override;

 private:
  bool handleScroll(double dx, double dy) override;
  const std::vector<std::string> getPulseIcon() const;

  std::shared_ptr<util::AudioBackend> backend = nullptr;
  util::PulseaudioTarget target = util::PulseaudioTarget::Sink;
};

}  // namespace waybar::modules
