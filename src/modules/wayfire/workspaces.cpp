#include "modules/wayfire/workspaces.hpp"

#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <spdlog/spdlog.h>

#include <string>
#include <utility>

#include "modules/wayfire/backend.hpp"

namespace waybar::modules::wayfire {

Workspaces::Workspaces(const std::string& id, const Bar& bar, const Json::Value& config)
    : AModule{config, "workspaces", id, false, !config["disable-scroll"].asBool()},
      ipc{IPC::get_instance()},
      handler{[this](const auto&) { dp.emit(); }},
      bar_{bar} {
  // init box_
  box_.set_name("workspaces");
  if (!id.empty()) box_.get_style_context()->add_class(id);
  box_.get_style_context()->add_class(MODULE_CLASS);
  event_box_.add(box_);

  // scroll events
  if (!config_["disable-scroll"].asBool()) {
    auto& target = config_["enable-bar-scroll"].asBool() ? const_cast<Bar&>(bar_).window
                                                         : dynamic_cast<Gtk::Widget&>(box_);
    target.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
    target.signal_scroll_event().connect(sigc::mem_fun(*this, &Workspaces::handleScroll));
  }

  // listen events
  ipc->register_handler("view-mapped", handler);
  ipc->register_handler("view-unmapped", handler);
  ipc->register_handler("view-wset-changed", handler);
  ipc->register_handler("output-gain-focus", handler);
  ipc->register_handler("view-sticky", handler);
  ipc->register_handler("view-workspace-changed", handler);
  ipc->register_handler("output-wset-changed", handler);
  ipc->register_handler("wset-workspace-changed", handler);

  ipc->register_handler("window-rules/list-views", handler);
  ipc->register_handler("window-rules/list-outputs", handler);
  ipc->register_handler("window-rules/list-wsets", handler);
  ipc->register_handler("window-rules/get-focused-output", handler);

  // initial render
  dp.emit();
}

Workspaces::~Workspaces() { ipc->unregister_handler(handler); }

auto Workspaces::handleScroll(GdkEventScroll* e) -> bool {
  // Ignore emulated scroll events on window
  if (gdk_event_get_pointer_emulated((GdkEvent*)e) != 0) return false;

  auto dir = AModule::getScrollDir(e);
  if (dir == SCROLL_DIR::NONE) return true;

  int delta;
  if (dir == SCROLL_DIR::DOWN || dir == SCROLL_DIR::RIGHT)
    delta = 1;
  else if (dir == SCROLL_DIR::UP || dir == SCROLL_DIR::LEFT)
    delta = -1;
  else
    return true;

  // cycle workspace
  Json::Value data;
  {
    auto _ = ipc->lock_state();
    const auto& output = ipc->get_outputs().at(bar_.output->name);
    const auto& wset = ipc->get_wsets().at(output.wset_idx);
    auto n = wset.ws_w * wset.ws_h;
    auto i = (wset.ws_idx() + delta + n) % n;
    data["x"] = i % wset.ws_w;
    data["y"] = i / wset.ws_h;
    data["output-id"] = output.id;
  }
  ipc->send("vswitch/set-workspace", std::move(data));

  return true;
}

auto Workspaces::update() -> void {
  update_box();
  AModule::update();
}

auto Workspaces::update_box() -> void {
  auto _ = ipc->lock_state();

  const auto& output_name = bar_.output->name;
  const auto& output = ipc->get_outputs().at(output_name);
  const auto& wset = ipc->get_wsets().at(output.wset_idx);

  auto output_focused = ipc->get_focused_output_name() == output_name;
  auto ws_w = wset.ws_w;
  auto ws_h = wset.ws_h;
  auto num_wss = ws_w * ws_h;

  // add buttons for new workspaces
  for (auto i = buttons_.size(); i < num_wss; i++) {
    auto& btn = buttons_.emplace_back("");
    box_.pack_start(btn, false, false, 0);
    btn.set_relief(Gtk::RELIEF_NONE);
    if (!config_["disable-click"].asBool()) {
      btn.signal_pressed().connect([=, this] {
        Json::Value data;
        data["x"] = i % ws_w;
        data["y"] = i / ws_h;
        data["output-id"] = output.id;
        ipc->send("vswitch/set-workspace", std::move(data));
      });
    }
  }

  // remove buttons for removed workspaces
  buttons_.resize(num_wss);

  // update buttons
  for (size_t i = 0; i < num_wss; i++) {
    const auto& ws = wset.wss[i];
    auto& btn = buttons_[i];
    auto ctx = btn.get_style_context();
    auto ws_focused = i == wset.ws_idx();
    auto ws_empty = ws.num_views == 0;

    // update #workspaces button.focused
    if (ws_focused)
      ctx->add_class("focused");
    else
      ctx->remove_class("focused");

    // update #workspaces button.empty
    if (ws_empty)
      ctx->add_class("empty");
    else
      ctx->remove_class("empty");

    // update #workspaces button.current_output
    if (output_focused)
      ctx->add_class("current_output");
    else
      ctx->remove_class("current_output");

    // update label
    auto label = std::to_string(i + 1);
    if (config_["format"].isString()) {
      auto format = config_["format"].asString();
      auto ws_idx = std::to_string(i + 1);

      const auto& icons = config_["format-icons"];
      std::string icon;
      if (!icons)
        icon = ws_idx;
      else if (ws_focused && icons["focused"])
        icon = icons["focused"].asString();
      else if (icons[ws_idx])
        icon = icons[ws_idx].asString();
      else if (icons["default"])
        icon = icons["default"].asString();
      else
        icon = ws_idx;

      label = fmt::format(fmt::runtime(format), fmt::arg("icon", icon), fmt::arg("index", ws_idx),
                          fmt::arg("output", output_name));
    }
    if (!config_["disable-markup"].asBool())
      static_cast<Gtk::Label*>(btn.get_children()[0])->set_markup(label);
    else
      btn.set_label(label);

    //
    if (config_["current-only"].asBool() && i != wset.ws_idx())
      btn.hide();
    else
      btn.show();
  }
}

}  // namespace waybar::modules::wayfire
