#pragma once

#include <mutex>
#include <string>

#include "ALabel.hpp"
#include "bar.hpp"
#include "modules/mango/backend.hpp"

namespace waybar::modules::mango {

class Keymode : public ALabel, public EventHandler {
 public:
  Keymode(const std::string&, const Bar&, const Json::Value&);
  ~Keymode() override;
  void update() override;

 private:
  void onEvent(const Json::Value& ev) override;
  void doUpdate();

  std::mutex mutex_;
  const Bar& bar_;
  std::string last_keymode_;
};

}  // namespace waybar::modules::mango