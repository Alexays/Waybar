#include "modules/wayfire/window.hpp"

#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <spdlog/spdlog.h>

#include "util/rewrite_string.hpp"
#include "util/sanitize_str.hpp"

namespace waybar::modules::wayfire {

Window::Window(const std::string& id, const Bar& bar, const Json::Value& config)
    : AAppIconLabel(config, "window", id, "{title}", 0, true),
      ipc{IPC::get_instance()},
      handler{[this](const auto&) { dp.emit(); }},
      bar_{bar} {
  ipc->register_handler("view-unmapped", handler);
  ipc->register_handler("view-focused", handler);
  ipc->register_handler("view-title-changed", handler);
  ipc->register_handler("view-app-id-changed", handler);

  ipc->register_handler("window-rules/get-focused-view", handler);

  dp.emit();
}

Window::~Window() { ipc->unregister_handler(handler); }

auto Window::update() -> void {
  update_icon_label();
  AAppIconLabel::update();
}

auto Window::update_icon_label() -> void {
  auto _ = ipc->lock_state();

  const auto& output = ipc->get_outputs().at(bar_.output->name);
  const auto& wset = ipc->get_wsets().at(output.wset_idx);
  const auto& views = ipc->get_views();
  auto ctx = bar_.window.get_style_context();

  if (views.contains(wset.focused_view_id)) {
    const auto& view = views.at(wset.focused_view_id);
    auto title = view["title"].asString();
    auto app_id = view["app-id"].asString();

    // update label
    label_.set_markup(waybar::util::rewriteString(
        fmt::format(fmt::runtime(format_), fmt::arg("title", waybar::util::sanitize_string(title)),
                    fmt::arg("app_id", waybar::util::sanitize_string(app_id))),
        config_["rewrite"]));

    // update window#waybar.solo
    if (wset.locate_ws(view["geometry"]).num_views > 1)
      ctx->remove_class("solo");
    else
      ctx->add_class("solo");

    // update window#waybar.<app_id>
    ctx->remove_class(old_app_id_);
    ctx->add_class(old_app_id_ = app_id);

    // update window#waybar.empty
    ctx->remove_class("empty");

    //
    updateAppIconName(app_id, "");
    label_.show();
  } else {
    ctx->add_class("empty");

    updateAppIconName("", "");
    label_.hide();
  }
}

}  // namespace waybar::modules::wayfire
