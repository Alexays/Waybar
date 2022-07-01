#include <fmt/format.h>

#include <tuple>

#include "ALabel.hpp"
#include "bar.hpp"
#include "modules/hyprland/backend.hpp"
#include "util/json.hpp"

namespace waybar::modules::hyprland {

class Window : public waybar::ALabel {
public:
  Window(const std::string&, const waybar::Bar&, const Json::Value&);
  ~Window() = default;

private:
  void onEvent(const std::string&);

  const Bar& bar_;
  IPC ipc;
  unsigned app_icon_size_{24};
  bool update_app_icon_{true};
  std::string app_icon_name_;
  util::JsonParser parser_;
  std::string lastView;
};

}