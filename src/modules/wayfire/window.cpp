#include "modules/wayfire/window.hpp"

#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <spdlog/spdlog.h>

#include "util/rewrite_string.hpp"
#include "util/sanitize_str.hpp"

namespace waybar::modules::wayfire {

Window::Window(const std::string& id, const Bar& bar, const Json::Value& config)
    : AAppIconLabel(config, "window", id, "{title}", 0, true),
      ipc_{IPC::get_instance()},
      handler_{[this](const auto&) { dp.emit(); }},
      bar_{bar} {
  ipc_->register_handler("view-unmapped", handler_);
  ipc_->register_handler("view-focused", handler_);
  ipc_->register_handler("view-title-changed", handler_);
  ipc_->register_handler("view-app-id-changed", handler_);
  ipc_->register_handler("window-rules/get-focused-view", handler_);
}

Window::~Window() { ipc_->unregister_handler(handler_); }

auto Window::doUpdate() -> void {
  update_icon_label();
  AAppIconLabel::doUpdate();
}

auto Window::update_icon_label() -> void {
  auto _ = ipc_->lock_state();

  auto out_it = ipc_->get_outputs().find(bar_.output->name);
  if (out_it == ipc_->get_outputs().end()) return;
  const auto& output = out_it->second;
  auto wset_it = ipc_->get_wsets().find(output.wset_idx);
  if (wset_it == ipc_->get_wsets().end()) return;
  const auto& wset = wset_it->second;
  const auto& views = ipc_->get_views();
  auto& ctx{const_cast<Bar&>(bar_).window};

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
      ctx.remove_css_class("solo");
    else
      ctx.add_css_class("solo");

    // update window#waybar.<app_id>
    ctx.remove_css_class(old_app_id_);
    ctx.add_css_class(old_app_id_ = app_id);

    // update window#waybar.empty
    ctx.remove_css_class("empty");

    //
    updateAppIconName(app_id, "");
    label_.show();
  } else {
    ctx.add_css_class("empty");

    updateAppIconName("", "");
    label_.hide();
  }
}

}  // namespace waybar::modules::wayfire
