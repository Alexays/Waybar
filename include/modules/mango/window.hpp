#pragma once

#include <gtkmm/button.h>
#include <json/value.h>

#include <mutex>

#include "AAppIconLabel.hpp"
#include "bar.hpp"
#include "modules/mango/backend.hpp"

namespace waybar::modules::mango {

class Window final : public AAppIconLabel, public EventHandler {
 public:
  Window(const std::string&, const Bar&, const Json::Value&);
  ~Window() override;

 private:
  void doUpdate() override;
  void onEvent(const Json::Value& ev) override;
  void setClass(const std::string& className, bool enable);

  const Bar& bar_;
  std::string oldAppId_;
  std::mutex mutex_;
};

}  // namespace waybar::modules::mango
