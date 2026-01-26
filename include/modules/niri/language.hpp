#pragma once

#include <string>

#include "ALabel.hpp"
#include "bar.hpp"
#include "modules/niri/backend.hpp"

namespace waybar::modules::niri {

class Language : public ALabel, public EventHandler {
 public:
  Language(const std::string &, const Bar &, const Json::Value &, std::mutex &, std::list<pid_t> &);
  ~Language() override;
  void update() override;

 private:
  void updateFromIPC();
  void onEvent(const Json::Value &ev) override;
  void doUpdate();

  struct Layout {
    std::string full_name;
    std::string short_name;
    std::string variant;
    std::string short_description;
  };

  static Layout getLayout(const std::string &fullName);

  std::mutex mutex_;
  const Bar &bar_;

  std::vector<Layout> layouts_;
  unsigned current_idx_;
  std::string last_short_name_;
};

}  // namespace waybar::modules::niri
