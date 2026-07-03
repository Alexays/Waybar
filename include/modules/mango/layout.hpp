#pragma once

#include <mutex>
#include <string>

#include "ALabel.hpp"
#include "bar.hpp"
#include "modules/mango/backend.hpp"

namespace waybar::modules::mango {

class Layout : public ALabel, public EventHandler {
 public:
  Layout(const std::string&, const Bar&, const Json::Value&);
  ~Layout() override;
  void update() override;

 private:
  void onEvent(const Json::Value& ev) override;
  void doUpdate();

  std::mutex mutex_;
  const Bar& bar_;
  std::string last_symbol_;
};

}  // namespace waybar::modules::mango