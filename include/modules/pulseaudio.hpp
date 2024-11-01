#pragma once

#include "ALabel.hpp"
#include "util/audio_backend.hpp"

namespace waybar::modules {

class Pulseaudio final : public ALabel {
 public:
  Pulseaudio(const std::string&, const Json::Value&);
  virtual ~Pulseaudio() = default;
  auto update() -> void override;

 private:
  bool handleScroll(double dx, double dy) override;
  const std::vector<std::string> getPulseIcon() const;

  std::shared_ptr<util::AudioBackend> backend = nullptr;
};

}  // namespace waybar::modules
