#include <fmt/format.h>

#include "ALabel.hpp"
#include "bar.hpp"
#include "modules/hyprland/backend.hpp"
#include "util/json.hpp"

namespace waybar::modules::hyprland {

class Language : public waybar::ALabel, public EventHandler {
 public:
  Language(const std::string&, const waybar::Bar&, const Json::Value&);
  virtual ~Language();

  auto update() -> void override;

 private:
  void onEvent(const std::string&) override;

  void initLanguage();
  std::string getShortFrom(const std::string&);

  std::mutex mutex_;
  const Bar& bar_;
  util::JsonParser parser_;
  std::string layoutName_;
};

}  // namespace waybar::modules::hyprland
