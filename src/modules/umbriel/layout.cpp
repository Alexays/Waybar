#include "modules/umbriel/layout.hpp"

#include <fmt/format.h>

#include <string>

namespace waybar::modules::umbriel {

Layout::Layout(const std::string& id, const Bar& bar, const Json::Value& config)
    : ALabel(config, "layout", id, "{}", 0, false), bar_(bar) {
  if (!gIPC) gIPC = std::make_unique<IPC>();

  gIPC->registerForIPC("workspaces", this);
}

Layout::~Layout() { gIPC->unregisterForIPC(this); }

void Layout::onEvent(const Json::Value& /*ev*/) { dp.emit(); }

void Layout::doUpdate() {
  auto ipcLock = gIPC->lockData();

  // Each bar belongs to one output, and Umbriel keeps exactly one active
  // workspace per output. Prefer the active workspace of this bar's output over
  // the globally focused workspace so multi-monitor bars stay independent.
  const Json::Value* workspace = nullptr;
  for (const auto& ws : gIPC->workspaces()) {
    if (ws["active"].asBool() && ws["output"].asString() == bar_.output->name) {
      workspace = &ws;
      break;
    }
  }

  const std::string layout = workspace != nullptr ? workspace->operator[]("layout").asString() : "";

  if (layout.empty()) {
    if (!last_layout_.empty()) {
      label_.get_style_context()->remove_class(last_layout_);
      last_layout_.clear();
    }
    label_.hide();
    return;
  }

  if (layout != last_layout_) {
    if (!last_layout_.empty()) {
      label_.get_style_context()->remove_class(last_layout_);
    }
    label_.get_style_context()->add_class(layout);
    last_layout_ = layout;
  }

  const auto format_key = "format-" + layout;
  const auto format = config_.isMember(format_key) && config_[format_key].isString()
                          ? config_[format_key].asString()
                          : format_;

  const auto text = fmt::format(fmt::runtime(format), fmt::arg("layout", layout));

  if (!text.empty()) {
    label_.show();
    label_.set_markup(text);
  } else {
    label_.hide();
  }
}

void Layout::update() {
  doUpdate();
  ALabel::update();
}

}  // namespace waybar::modules::umbriel
