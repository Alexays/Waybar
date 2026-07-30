#include "modules/mango/window.hpp"

#include <spdlog/spdlog.h>

#include "util/rewrite_string.hpp"
#include "util/sanitize_str.hpp"

namespace waybar::modules::mango {

Window::Window(const std::string& id, const Bar& bar, const Json::Value& config)
    : AAppIconLabel(config, "window", id, "{title}", 0, true), bar_(bar) {
  IPC::getInstance().registerForIPC("monitor", this);
}

Window::~Window() { IPC::getInstance().unregisterForIPC(this); }

void Window::onEvent(const Json::Value& ev) { dp.emit(); }

void Window::doUpdate() {
  std::lock_guard<std::mutex> lock(mutex_);

  const Json::Value& client = IPC::getInstance().getActiveClientForMonitor(bar_.output->name);

  // judge whether to hide: active_client is null or title field is null
  if (client.isNull() || !client.isObject() || client["title"].isNull()) {
    event_box_.hide();
    label_.hide();
    updateAppIconName("", "");
    setClass("empty", true);
    if (!oldAppId_.empty()) setClass(oldAppId_, false);
    oldAppId_.clear();
    return;
  }

  // if we have a valid client, show the label and update content
  event_box_.show();
  label_.show();
  setClass("empty", false);

  std::string title = client["title"].asString();
  std::string appid = client["appid"].asString();
  std::string sanitized_title = waybar::util::sanitize_string(title);
  std::string sanitized_appid = waybar::util::sanitize_string(appid);

  label_.set_markup(waybar::util::rewriteString(
      fmt::format(fmt::runtime(format_), fmt::arg("title", sanitized_title),
                  fmt::arg("app_id", sanitized_appid)),
      config_["rewrite"]));

  updateAppIconName(appid, "");
  if (tooltipEnabled()) label_.set_tooltip_markup(title);

  // Solo judgment
  bool solo = false;
  if (client.isMember("tags") && client["tags"].isArray() && client["tags"].size() == 1) {
    int tag_idx = client["tags"][0].asInt();
    const auto& monitors = IPC::getInstance().getMonitors();
    auto mon_it = monitors.find(client["monitor"].asString());
    if (mon_it != monitors.end()) {
      const auto& tags = mon_it->second["tags"];
      for (const auto& tag : tags) {
        if (tag["index"].asInt() == tag_idx) {
          solo = (tag["client_count"].asInt() == 1);
          break;
        }
      }
    }
  }
  setClass("solo", solo);
  if (!appid.empty()) setClass(appid, solo);

  if (oldAppId_ != appid) {
    if (!oldAppId_.empty()) setClass(oldAppId_, false);
    oldAppId_ = appid;
  }
}

void Window::update() {
  doUpdate();
  AAppIconLabel::update();
}

void Window::setClass(const std::string& className, bool enable) {
  auto style_context = event_box_.get_style_context();
  if (enable) {
    if (!style_context->has_class(className)) style_context->add_class(className);
  } else {
    style_context->remove_class(className);
  }
}

}  // namespace waybar::modules::mango