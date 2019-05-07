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
  ipc_.subscribe(R"(["window","workspace"])");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &Window::onEvent));
  ipc_.signal_cmd.connect(sigc::mem_fun(*this, &Window::onCmd));
  // Get Initial focused window
  getTree();
  // Launch worker
  worker();
}

void Window::onEvent(const struct Ipc::ipc_response& res) { getTree(); }

void Window::onCmd(const struct Ipc::ipc_response& res) {
  auto payload = parser_.parse(res.payload);
  auto [nb, id, name, app_id] = getFocusedNode(payload);
  if (!app_id_.empty()) {
    bar_.window.get_style_context()->remove_class(app_id_);
  }
  if (nb == 0) {
    bar_.window.get_style_context()->add_class("empty");
  } else if (nb == 1) {
    bar_.window.get_style_context()->add_class("solo");
    if (!app_id.empty()) {
      bar_.window.get_style_context()->add_class(app_id);
    }
  } else {
    bar_.window.get_style_context()->remove_class("solo");
    bar_.window.get_style_context()->remove_class("empty");
  }
  app_id_ = app_id;
  if (windowId_ != id || window_ != name) {
    windowId_ = id;
    window_ = name;
    dp.emit();
  }
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

std::tuple<std::size_t, int, std::string, std::string> Window::getFocusedNode(
    const Json::Value& nodes) {
  for (auto const& node : nodes["nodes"]) {
    if (node["focused"].asBool() && node["type"] == "con") {
      if ((!config_["all-outputs"].asBool() && nodes["output"] == bar_.output->name) ||
          config_["all-outputs"].asBool()) {
        auto app_id = node["app_id"].isString() ? node["app_id"].asString()
                                                : node["window_properties"]["instance"].asString();
        return {nodes["nodes"].size(),
                node["id"].asInt(),
                Glib::Markup::escape_text(node["name"].asString()),
                app_id};
      }
    }
    auto [nb, id, name, app_id] = getFocusedNode(node);
    if (id > -1 && !name.empty()) {
      return {nb, id, name, app_id};
    }
  }
  return {0, -1, "", ""};
}

void Window::getTree() {
  try {
    ipc_.sendCmd(IPC_GET_TREE);
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
}

}  // namespace waybar::modules::sway