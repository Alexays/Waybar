#pragma once

#include <sigc++/connection.h>

#include <map>
#include <string>

#include "ALabel.hpp"
#include "cava_backend.hpp"

namespace waybar::modules::cava {

class CavaASCII final : public ALabel {
 public:
  CavaASCII(const std::string&, const Json::Value&);
  ~CavaASCII();
  auto doAction(const std::string& name) -> void override;

 private:
  // Cava method
  void pauseResume();
  auto onUpdate(const std::string& input) -> void;
  auto onSilence() -> void;

  using Action = void (CavaASCII::*)();

  std::shared_ptr<CavaBackend> backend_;
  // Text to display
  Glib::ustring label_text_;
  bool silence_{false};
  bool hide_on_silence_{false};
  std::string format_silent_;
  // ModuleActionMap
  static const std::map<std::string, Action> actionMap_;

  sigc::connection update_conn_;
  sigc::connection silence_conn_;
};
}  // namespace waybar::modules::cava
