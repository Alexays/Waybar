#pragma once

#include <map>
#include <string>

#include <sigc++/connection.h>

#include "ALabel.hpp"
#include "cava_backend.hpp"

namespace waybar::modules::cava {

class CavaRaw final : public ALabel {
 public:
  CavaRaw(const std::string&, const Json::Value&);
  ~CavaRaw();
  auto doAction(const std::string& name) -> void override;

 private:
  using Action = void (CavaRaw::*)();

  std::shared_ptr<CavaBackend> backend_;
  // Text to display
  Glib::ustring label_text_;
  bool silence_{false};
  bool hide_on_silence_{false};
  std::string format_silent_;
  // Cava method
  void pauseResume();
  auto onUpdate(const std::string& input) -> void;
  auto onSilence() -> void;
  // ModuleActionMap
  static const std::map<std::string, Action> actionMap_;

  sigc::connection update_conn_;
  sigc::connection silence_conn_;
};
}  // namespace waybar::modules::cava
