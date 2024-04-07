#pragma once

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <memory>

#include "ALabel.hpp"
#include "util/audio_backend.hpp"

namespace wabar::modules {

class Pulseaudio : public ALabel {
 public:
  Pulseaudio(const std::string&, const Json::Value&);
  virtual ~Pulseaudio() = default;
  auto update() -> void override;

 private:
  bool handleScroll(GdkEventScroll* e) override;
  const std::vector<std::string> getPulseIcon() const;

  std::shared_ptr<util::AudioBackend> backend = nullptr;
};

}  // namespace wabar::modules
