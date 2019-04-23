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
  auto data = res.payload;
  // Check for waybar prevents flicker when hovering window module
  if ((data["change"] == "focus" || data["change"] == "title") &&
      data["container"]["focused"].asBool() && data["container"]["name"].asString() != "waybar") {
    window_ = Glib::Markup::escape_text(data["container"]["name"].asString());
    windowId_ = data["container"]["id"].asInt();
    dp.emit();
  } else if ((data["change"] == "close" && data["container"]["focused"].asBool() &&
              windowId_ == data["container"]["id"].asInt()) ||
             (data["change"] == "focus" && data["current"]["focus"].isArray() &&
              data["current"]["focus"].empty())) {
    window_.clear();
    windowId_ = -1;
    dp.emit();
  }
}

void Window::onCmd(const struct Ipc::ipc_response res) {
  auto [id, name] = getFocusedNode(res.payload["nodes"]);
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