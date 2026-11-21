#pragma once

#include <string>

#include "ALabel.hpp"
#include "bar.hpp"
#include "modules/umbriel/backend.hpp"

namespace waybar::modules::umbriel {

class Layout : public ALabel, public EventHandler {
 public:
  Layout(const std::string&, const Bar&, const Json::Value&);
  ~Layout() override;
  void update() override;

 private:
  void onEvent(const Json::Value& ev) override;
  void doUpdate();

  const Bar& bar_;
  std::string last_layout_;
};

}  // namespace waybar::modules::umbriel
