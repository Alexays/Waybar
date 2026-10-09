#pragma once

#include <mutex>
#include <string>

#include "ALabel.hpp"
#include "bar.hpp"
#include "modules/mango/backend.hpp"

namespace waybar::modules::mango {

class Layout final : public ALabel, public EventHandler {
 public:
  Layout(const std::string&, const Bar&, const Json::Value&);
  ~Layout() override;

 private:
  void doUpdate() override;
  void onEvent(const Json::Value& ev) override;

  std::mutex mutex_;
  const Bar& bar_;
  std::string last_symbol_;
};

}  // namespace waybar::modules::mango
