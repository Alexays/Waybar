#pragma once

#include <json/value.h>

#include <string>

#include "AAppIconLabel.hpp"
#include "bar.hpp"
#include "modules/umbriel/backend.hpp"

namespace waybar::modules::umbriel {

class Window : public AAppIconLabel, public EventHandler {
 public:
  Window(const std::string&, const Bar&, const Json::Value&);
  ~Window() override;
  void update() override;

 private:
  void onEvent(const Json::Value& ev) override;
  void doUpdate();
  void setClass(const std::string& className, bool enable);

  const Bar& bar_;
  std::string oldAppId_;
};

}  // namespace waybar::modules::umbriel
