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
      ipc_{IPC::get_instance()},
      handler_{[this](const auto&) { dp.emit(); }},
      bar_{bar} {
  w_ = &box_;
  // init box_
  box_.set_name("workspaces");
  if (!id.empty()) box_.get_style_context()->add_class(id);
  box_.get_style_context()->add_class(MODULE_CLASS);

  // listen events
  ipc_->register_handler("view-mapped", handler_);
  ipc_->register_handler("view-unmapped", handler_);
  ipc_->register_handler("view-wset-changed", handler_);
  ipc_->register_handler("output-gain-focus", handler_);
  ipc_->register_handler("view-sticky", handler_);
  ipc_->register_handler("view-workspace-changed", handler_);
  ipc_->register_handler("output-wset-changed", handler_);
  ipc_->register_handler("wset-workspace-changed", handler_);

  ipc_->register_handler("window-rules/list-views", handler_);
  ipc_->register_handler("window-rules/list-outputs", handler_);
  ipc_->register_handler("window-rules/list-wsets", handler_);
  ipc_->register_handler("window-rules/get-focused-output", handler_);

  bindEvents(box_);
  controller_scroll_->set_propagation_phase(Gtk::PropagationPhase::BUBBLE);
}

Workspaces::~Workspaces() { ipc_->unregister_handler(handler_); }

auto Workspaces::handleScroll(double dx, double dy) -> bool {
  const auto e{controller_scroll_->get_current_event()};
  // Ignore emulated scroll events on window
  if (auto device{e->get_device()}) {
    if (device->get_source() == Gdk::InputSource::TOUCHSCREEN) {
      return false;
    }
  }

  auto dir{AModule::getScrollDir(e)};
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
    auto _ = ipc_->lock_state();
    auto out_it = ipc_->get_outputs().find(bar_.output->name);
    if (out_it == ipc_->get_outputs().end()) return true;
    const auto& output = out_it->second;
    auto wset_it = ipc_->get_wsets().find(output.wset_idx);
    if (wset_it == ipc_->get_wsets().end()) return true;
    const auto& wset = wset_it->second;
    auto n = wset.ws_w * wset.ws_h;
    auto i = (wset.ws_idx() + delta + n) % n;
    data["x"] = Json::Value((uint64_t)i % wset.ws_w);
    data["y"] = Json::Value((uint64_t)i / wset.ws_h);
    data["output-id"] = Json::Value((uint64_t)output.id);
  }
  ipc_->send("vswitch/set-workspace", std::move(data));

  return true;
}

auto Workspaces::doUpdate() -> void {
  update_box();
  AModule::doUpdate();
}

auto Workspaces::update_box() -> void {
  auto _ = ipc_->lock_state();

  const auto& output_name = bar_.output->name;
  auto out_it = ipc_->get_outputs().find(output_name);
  if (out_it == ipc_->get_outputs().end()) return;
  const auto& output = out_it->second;
  auto wset_it = ipc_->get_wsets().find(output.wset_idx);
  if (wset_it == ipc_->get_wsets().end()) return;
  const auto& wset = wset_it->second;

  auto output_focused = ipc_->get_focused_output_name() == output_name;
  auto ws_w = wset.ws_w;
  auto ws_h = wset.ws_h;
  auto num_wss = ws_w * ws_h;

  // add buttons for new workspaces
  for (auto i = buttons_.size(); i < num_wss; i++) {
    auto& btn = buttons_.emplace_back("");
    box_.append(btn);
    btn.add_css_class("flat");
    if (!config_["disable-click"].asBool()) {
      auto gesture_click{Gtk::GestureClick::create()};
      gesture_click->set_propagation_phase(Gtk::PropagationPhase::TARGET);
      gesture_click->set_button(0u);
      btn.add_controller(gesture_click);
      gesture_click->signal_pressed().connect([=, this](int n_press, double x, double y) {
        Json::Value data;
        data["x"] = Json::Value((uint64_t)i % ws_w);
        data["y"] = Json::Value((uint64_t)i / ws_h);
        data["output-id"] = Json::Value((uint64_t)output.id);
        ipc_->send("vswitch/set-workspace", std::move(data));
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
