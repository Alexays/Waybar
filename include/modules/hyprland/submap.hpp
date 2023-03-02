#include <fmt/format.h>

#include "ALabel.hpp"
#include "bar.hpp"
#include "modules/hyprland/backend.hpp"
#include "util/json.hpp"

namespace waybar::modules::hyprland {

class Submap : public waybar::ALabel, public EventHandler {
 public:
  Submap(const std::string&, const waybar::Bar&, const Json::Value&);
  virtual ~Submap();

  auto update() -> void override;

 private:
  void onEvent(const std::string&) override;

  std::mutex mutex_;
  const Bar& bar_;
  util::JsonParser parser_;
  std::string submap_;
};

}  // namespace waybar::modules::hyprland
