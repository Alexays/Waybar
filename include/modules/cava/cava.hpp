#pragma once

#include "ALabel.hpp"
#include "cava_backend.hpp"

namespace waybar::modules::cava {

class Cava final : public ALabel, public sigc::trackable {
 public:
  Cava(const std::string&, const Json::Value&);
  ~Cava() = default;
  auto onUpdate(const std::string& input) -> void;
  auto onSilence() -> void;
  auto doAction(const std::string& name) -> void override;

 private:
  std::shared_ptr<CavaBackend> backend_;
  // Text to display
  std::string label_text_{""};
  bool hide_on_silence_{false};
  std::string format_silent_{""};
  int ascii_range_{0};
  bool silence_{false};
  // Cava method
  void pause_resume();
  // ModuleActionMap
  static inline std::map<const std::string, void (waybar::modules::cava::Cava::* const)()>
      actionMap_{{"mode", &waybar::modules::cava::Cava::pause_resume}};
};
}  // namespace waybar::modules::cava
