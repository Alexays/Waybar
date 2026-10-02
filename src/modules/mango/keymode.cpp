#include "modules/mango/keymode.hpp"

#include <spdlog/spdlog.h>

namespace waybar::modules::mango {

Keymode::Keymode(const std::string& id, const Bar& bar, const Json::Value& config, std::mutex& reap_mtx,
		 std::list<pid_t>& reap)
    : ALabel(config, "keymode", id, "{}", reap_mtx, reap, 0, false), bar_(bar) {
  IPC::getInstance().registerForIPC("monitor", this);
}

Keymode::~Keymode() { IPC::getInstance().unregisterForIPC(this); }

void Keymode::onEvent(const Json::Value& ev) { dp.emit(); }

void Keymode::doUpdate() {
  std::lock_guard<std::mutex> lock(mutex_);

  std::string current = IPC::getInstance().getKeymode();

  // if keymode is empty, hide the label
  if (current.empty()) {
    label_.hide();
    last_keymode_.clear();
    return;
  }

  // if keymode is the same as last time, skip style changes
  if (current != last_keymode_) {
    if (!last_keymode_.empty()) label_.get_style_context()->remove_class(last_keymode_);
    label_.get_style_context()->add_class(current);
    last_keymode_ = current;
  }

  // support config's format-keymode custom format (such as format-default, format-resize, etc.)
  std::string text;
  std::string format_key = "format-" + current;
  if (config_.isMember(format_key)) {
    text = fmt::format(fmt::runtime(config_[format_key].asString()), fmt::arg("mode", current));
  } else {
    text = fmt::format(fmt::runtime(format_), fmt::arg("mode", current));
  }

  if (!text.empty()) {
    label_.show();
    label_.set_markup(text);
  } else {
    label_.hide();
  }
}

void Keymode::update() {
  doUpdate();
  ALabel::update();
}

}  // namespace waybar::modules::mango
