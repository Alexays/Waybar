#pragma once

#include <mutex>
#include <string>

#include "ALabel.hpp"
#include "bar.hpp"
#include "modules/mango/backend.hpp"

namespace waybar::modules::mango {

class Keymode final : public ALabel, public EventHandler {
 public:
  Keymode(const std::string&, const Bar&, const Json::Value&);
  ~Keymode() override;

 private:
  void doUpdate() override;
  void onEvent(const Json::Value& ev) override;

  std::mutex mutex_;
  const Bar& bar_;
  std::string last_keymode_;
};

}  // namespace waybar::modules::mango
