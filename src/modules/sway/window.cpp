#include "modules/sway/window.hpp"

namespace waybar::modules::sway {

Window::Window(const std::string& id, const Bar& bar, const Json::Value& config)
    : ALabel(config, "{}"), bar_(bar), windowId_(-1) {
  label_.set_name("window");
  if (!id.empty()) {
    label_.get_style_context()->add_class(id);
  }
  if (label_.get_max_width_chars() == -1) {
    label_.set_hexpand(true);
    label_.set_ellipsize(Pango::EllipsizeMode::ELLIPSIZE_END);
  }
  ipc_.subscribe("[\"window\",\"workspace\"]");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &Window::onEvent));
  ipc_.signal_cmd.connect(sigc::mem_fun(*this, &Window::onCmd));
  getFocusedWindow();
  // Launch worker
  worker();
}

void Window::onEvent(const struct Ipc::ipc_response res) {
  auto parsed = parser_.parse(res.payload);
  // Check for waybar prevents flicker when hovering window module
  if ((parsed["change"] == "focus" || parsed["change"] == "title") &&
      parsed["container"]["focused"].asBool() &&
      parsed["container"]["name"].asString() != "waybar") {
    window_ = Glib::Markup::escape_text(parsed["container"]["name"].asString());
    windowId_ = parsed["container"]["id"].asInt();
    dp.emit();
  } else if ((parsed["change"] == "close" && parsed["container"]["focused"].asBool() &&
              windowId_ == parsed["container"]["id"].asInt()) ||
             (parsed["change"] == "focus" && parsed["current"]["focus"].isArray() &&
              parsed["current"]["focus"].empty())) {
    window_.clear();
    windowId_ = -1;
    dp.emit();
  }
}

void Window::onCmd(const struct Ipc::ipc_response res) {
  auto parsed = parser_.parse(res.payload);
  auto [id, name] = getFocusedNode(parsed["nodes"]);
  windowId_ = id;
  window_ = name;
  dp.emit();
}

void Window::worker() {
  thread_ = [this] {
    try {
      ipc_.handleEvent();
    } catch (const std::exception& e) {
      std::cerr << "Window: " << e.what() << std::endl;
    }
  };
}

auto Window::update() -> void {
  label_.set_markup(fmt::format(format_, window_));
  if (tooltipEnabled()) {
    label_.set_tooltip_text(window_);
  }
}

std::tuple<int, std::string> Window::getFocusedNode(Json::Value nodes) {
  for (auto const& node : nodes) {
    if (node["focused"].asBool() && node["type"] == "con") {
      return {node["id"].asInt(), node["name"].asString()};
    }
    auto [id, name] = getFocusedNode(node["nodes"]);
    if (id > -1 && !name.empty()) {
      return {id, name};
    }
  }
  return {-1, std::string()};
}

void Window::getFocusedWindow() {
  try {
    ipc_.sendCmd(IPC_GET_TREE);
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
}

}  // namespace waybar::modules::sway