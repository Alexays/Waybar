#include "modules/sway/window.hpp"

waybar::modules::sway::Window::Window(Bar &bar, const Json::Value& config)
  : ALabel(config, "{}"), bar_(bar)
{
  label_.set_name("window");
  ipc_.connect();
  ipc_.subscribe("[ \"window\" ]");
  getFocusedWindow();
  // Launch worker
  worker();
}

void waybar::modules::sway::Window::worker()
{
  thread_ = [this] {
    try {
      auto res = ipc_.handleEvent();
      auto parsed = parser_.parse(res.payload);
      if ((parsed["change"] == "focus" || parsed["change"] == "title")
        && parsed["container"]["focused"].asBool()) {
        window_ = parsed["container"]["name"].asString();
        dp.emit();
      }
    } catch (const std::exception& e) {
      std::cerr << e.what() << std::endl;
    }
  };
}

auto waybar::modules::sway::Window::update() -> void
{
  label_.set_text(fmt::format(format_, window_));
  label_.set_tooltip_text(window_);
}

std::string waybar::modules::sway::Window::getFocusedNode(Json::Value nodes)
{
  for (auto const& node : nodes) {
    if (node["focused"].asBool() && node["type"] == "con") {
      return node["name"].asString();
    }
    auto res = getFocusedNode(node["nodes"]);
    if (!res.empty()) {
      return res;
    }
  }
  return std::string();
}

void waybar::modules::sway::Window::getFocusedWindow()
{
  try {
    auto res = ipc_.sendCmd(IPC_GET_TREE);
    auto parsed = parser_.parse(res.payload);
    window_ = getFocusedNode(parsed["nodes"]);
    Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Window::update));
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}
