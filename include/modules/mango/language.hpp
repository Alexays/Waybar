#pragma once

#include <xkbcommon/xkbregistry.h>

#include <string>

#include "ALabel.hpp"
#include "bar.hpp"
#include "modules/mango/backend.hpp"

namespace waybar::modules::mango {

class Language : public ALabel, public EventHandler {
 public:
  Language(const std::string&, const Bar&, const Json::Value&, std::mutex&, std::list<pid_t>&);
  ~Language() override;
  void update() override;

 private:
  void updateFromIPC();
  void onEvent(const Json::Value& ev) override;
  void doUpdate();

  struct Layout {
    std::string full_name;
    std::string short_name;
    std::string variant;
    std::string short_description;
  };

  Layout getLayout(const std::string& fullName);

  std::mutex mutex_;
  const Bar& bar_;

  std::vector<Layout> layouts_;
  unsigned current_idx_;
  std::string last_short_name_;

  struct rxkb_context* rxkb_ctx_ = nullptr;
};

}  // namespace waybar::modules::mango
