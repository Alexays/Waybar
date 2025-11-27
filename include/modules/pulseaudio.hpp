#pragma once

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <memory>

#include "ALabel.hpp"
#include "util/audio_backend.hpp"

namespace waybar::modules {

class Pulseaudio : public ALabel {
 public:
  Pulseaudio(const std::string&, const Json::Value&,
             std::mutex&, std::list<pid_t>&);
  virtual ~Pulseaudio() = default;
  auto update() -> void override;

 private:
  bool handleScroll(GdkEventScroll* e) override;
  const std::vector<std::string> getPulseIcon() const;

  std::shared_ptr<util::AudioBackend> backend = nullptr;
};

}  // namespace waybar::modules
