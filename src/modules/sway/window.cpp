#include "modules/sway/window.hpp"

waybar::modules::sway::Window::Window(Bar &bar, const Json::Value& config)
  : ALabel(config, "{}"), bar_(bar), windowId_(-1)
{
  label_.set_name("window");
  if (label_.get_max_width_chars() == -1) {
    label_.set_hexpand(true);
    label_.set_ellipsize(Pango::EllipsizeMode::ELLIPSIZE_END);
  }
  ipc_.connect();
  ipc_.subscribe("[\"window\",\"workspace\"]");
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
        windowId_ = parsed["container"]["id"].asInt();
        dp.emit();
      } else if ((parsed["change"] == "close"
        && parsed["container"]["focused"].asBool()
        && windowId_ == parsed["container"]["id"].asInt())
        || (parsed["change"] == "focus" && parsed["current"]["focus"].isArray()
        && parsed["current"]["focus"].empty())) {
        window_.clear();
        windowId_ = -1;
        dp.emit();
      }
    } catch (const std::exception& e) {
      std::cerr << e.what() << std::endl;
    }
  };
}

auto waybar::modules::sway::Window::update() -> void
{
  label_.set_markup(fmt::format(format_, window_));
  label_.set_tooltip_text(window_);
}

std::tuple<int, std::string> waybar::modules::sway::Window::getFocusedNode(
  Json::Value nodes)
{
  for (auto const& node : nodes) {
    if (node["focused"].asBool() && node["type"] == "con") {
      return { node["id"].asInt(), node["name"].asString() };
    }
    auto [id, name] = getFocusedNode(node["nodes"]);
    if (id > -1 && !name.empty()) {
      return { id, name };
    }
  }
  return { -1, std::string() };
}

void waybar::modules::sway::Window::getFocusedWindow()
{
  try {
    auto res = ipc_.sendCmd(IPC_GET_TREE);
    auto parsed = parser_.parse(res.payload);
    auto [id, name] = getFocusedNode(parsed["nodes"]);
    windowId_ = id;
    window_ = name;
    Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Window::update));
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}
