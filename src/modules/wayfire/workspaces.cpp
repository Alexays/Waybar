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
  if (!id.empty()) box_.add_css_class(id);
  box_.add_css_class(MODULE_CLASS);
  event_box_.set_child(box_);
  enableChildMultiClick();

  // Scrolls over the module itself reach handleScroll through AModule and stop there
  if (!config_["disable-scroll"].asBool() && config_["enable-bar-scroll"].asBool()) {
    enableBarScroll(const_cast<Bar&>(bar_).window);
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
}

Workspaces::~Workspaces() { ipc->unregister_handler(handler); }

auto Workspaces::handleScroll(const ScrollEvent& e) -> bool {
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
    auto out_it = ipc->get_outputs().find(bar_.output->name);
    if (out_it == ipc->get_outputs().end()) return true;
    const auto& output = out_it->second;
    auto wset_it = ipc->get_wsets().find(output.wset_idx);
    if (wset_it == ipc->get_wsets().end()) return true;
    const auto& wset = wset_it->second;
    auto n = wset.ws_w * wset.ws_h;
    auto i = (wset.ws_idx() + delta + n) % n;
    data["x"] = Json::Value((uint64_t)i % wset.ws_w);
    data["y"] = Json::Value((uint64_t)i / wset.ws_w);
    data["output-id"] = Json::Value((uint64_t)output.id);
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
  auto out_it = ipc->get_outputs().find(output_name);
  if (out_it == ipc->get_outputs().end()) return;
  const auto& output = out_it->second;
  auto wset_it = ipc->get_wsets().find(output.wset_idx);
  if (wset_it == ipc->get_wsets().end()) return;
  const auto& wset = wset_it->second;

  auto output_focused = ipc->get_focused_output_name() == output_name;
  auto ws_w = wset.ws_w;
  auto ws_h = wset.ws_h;
  auto num_wss = ws_w * ws_h;

  // add buttons for new workspaces
  for (auto i = buttons_.size(); i < num_wss; i++) {
    auto& btn = buttons_.emplace_back("");
    box_.append(btn);
    btn.set_has_frame(false);
    if (!config_["disable-click"].asBool()) {
      // Acts on press like GtkButton::pressed: capture runs before the button claims the click
      auto press = Gtk::GestureClick::create();
      press->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
      press->signal_pressed().connect([=, this](int, double, double) {
        Json::Value data;
        data["x"] = Json::Value((uint64_t)i % ws_w);
        data["y"] = Json::Value((uint64_t)i / ws_w);
        data["output-id"] = Json::Value((uint64_t)output.id);
        ipc->send("vswitch/set-workspace", std::move(data));
      });
      btn.add_controller(press);
    }
  }

  // remove buttons for removed workspaces
  for (auto i = num_wss; i < buttons_.size(); i++) {
    box_.remove(buttons_[i]);
  }
  buttons_.resize(num_wss);

  // update buttons
  for (size_t i = 0; i < num_wss; i++) {
    const auto& ws = wset.wss[i];
    auto& btn = buttons_[i];
    auto ws_focused = i == wset.ws_idx();
    auto ws_empty = ws.num_views == 0;

    // update #workspaces button.focused
    if (ws_focused)
      btn.add_css_class("focused");
    else
      btn.remove_css_class("focused");

    // update #workspaces button.empty
    if (ws_empty)
      btn.add_css_class("empty");
    else
      btn.remove_css_class("empty");

    // update #workspaces button.current_output
    if (output_focused)
      btn.add_css_class("current_output");
    else
      btn.remove_css_class("current_output");

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
      static_cast<Gtk::Label*>(btn.get_child())->set_markup(label);
    else
      btn.set_label(label);

    //
    btn.set_visible(!config_["current-only"].asBool() || i == wset.ws_idx());
  }
}

}  // namespace waybar::modules::wayfire
