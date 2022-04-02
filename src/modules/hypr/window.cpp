#include "modules/hypr/window.hpp"
#include "modules/hypr/ipc.hpp"

using namespace waybar::util;

waybar::modules::hypr::Window::Window(const std::string& id, const Json::Value& config) : ALabel(config, "window", id, "{window}", 0.5f) {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

auto waybar::modules::hypr::Window::update() -> void {
  auto format = format_;

  std::string windowName = waybar::modules::hypr::makeRequest("activewindow");

  if (windowName != "")
     windowName = windowName.substr(windowName.find_first_of('>') + 2, windowName.find_first_of('\n') - windowName.find_first_of('>') - 3);

  event_box_.show();
  label_.set_markup(fmt::format(format,
                                fmt::arg("window", windowName)));

  // Call parent update
  ALabel::update();
}
