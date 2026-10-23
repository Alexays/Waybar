#pragma once

#include <gtkmm/button.h>
#include <json/value.h>

#include "AAppIconLabel.hpp"
#include "bar.hpp"
#include "modules/niri/backend.hpp"

namespace waybar::modules::niri {

class Window final : public AAppIconLabel, public EventHandler {
 public:
  Window(const std::string&, const Bar&, const Json::Value&);
  ~Window() override;
  void doUpdate() override;

 private:
  void onEvent(const Json::Value& ev) override;
  void setClass(const std::string& className, bool enable);

  const Bar& bar_;

  std::string oldAppId_;
};

}  // namespace waybar::modules::niri
