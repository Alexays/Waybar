#include "modules/mango/layout.hpp"

#include <spdlog/spdlog.h>

namespace waybar::modules::mango {

Layout::Layout(const std::string& id, const Bar& bar, const Json::Value& config,
               std::mutex& reap_mtx, std::list<pid_t>& reap)
    : ALabel(config, "layout", id, "{}", reap_mtx, reap, 0, false), bar_(bar) {
  IPC::getInstance().registerForIPC("monitor", this);
}

Layout::~Layout() { IPC::getInstance().unregisterForIPC(this); }

void Layout::onEvent(const Json::Value& ev) { dp.emit(); }

void Layout::doUpdate() {
  std::lock_guard<std::mutex> lock(mutex_);

  std::string symbol = IPC::getInstance().getLayoutSymbolForMonitor(bar_.output->name);

  if (symbol.empty()) {
    label_.hide();
    last_symbol_.clear();
    return;
  }

  if (symbol != last_symbol_) {
    if (!last_symbol_.empty()) label_.get_style_context()->remove_class(last_symbol_);
    label_.get_style_context()->add_class(symbol);
    last_symbol_ = symbol;
  }

  std::string text;
  std::string format_key = "format-" + symbol;

  if (config_.isMember(format_key)) {
    text = fmt::format(fmt::runtime(config_[format_key].asString()), fmt::arg("symbol", symbol));
  } else {
    text = fmt::format(fmt::runtime(format_), fmt::arg("symbol", symbol));
  }

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

}  // namespace waybar::modules::mango
