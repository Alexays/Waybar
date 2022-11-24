#include <fmt/format.h>

#include <tuple>

#include "ALabel.hpp"
#include "bar.hpp"
#include "modules/hyprland/backend.hpp"
#include "util/json.hpp"

namespace waybar::modules::hyprland {

class Window : public waybar::ALabel, public EventHandler {
 public:
  Window(const std::string&, const waybar::Bar&, const Json::Value&);
  ~Window();

  auto update() -> void;

 private:
  int getActiveWorkspaceID(std::string);
  std::string getLastWindowTitle(int);
  void onEvent(const std::string&);

  bool separate_outputs;
  std::mutex mutex_;
  const Bar& bar_;
  util::JsonParser parser_;
  std::string lastView;
};

}  // namespace waybar::modules::hyprland
