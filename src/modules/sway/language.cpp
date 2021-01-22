#include "modules/sway/language.hpp"
#include <spdlog/spdlog.h>

namespace waybar::modules::sway {

Language::Language(const std::string& id, const Json::Value& config)
    : ALabel(config, "language", id, "{}", 0, true) {
  ipc_.subscribe(R"(["input"])");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &Language::onEvent));
  ipc_.signal_cmd.connect(sigc::mem_fun(*this, &Language::onCmd));
  ipc_.sendCmd(IPC_GET_INPUTS);
  // Launch worker
  ipc_.setWorker([this] {
    try {
      ipc_.handleEvent();
    } catch (const std::exception& e) {
      spdlog::error("Language: {}", e.what());
    }
  });
  dp.emit();
}

void Language::onCmd(const struct Ipc::ipc_response& res) {
  try {
    auto payload = parser_.parse(res.payload);
    //Display current layout of a device with a maximum count of layouts, expecting that all will be OK
    Json::Value::ArrayIndex maxId = 0, max = 0;
    for(Json::Value::ArrayIndex i = 0; i < payload.size(); i++) {
      if(payload[i]["xkb_layout_names"].size() > max) {
        max = payload[i]["xkb_layout_names"].size();
        maxId = i;
      }
    }
    auto layout_name = payload[maxId]["xkb_active_layout_name"].asString().substr(0,2);
    lang_ = Glib::Markup::escape_text(layout_name);
    dp.emit();
  } catch (const std::exception& e) {
    spdlog::error("Language: {}", e.what());
  }
}

void Language::onEvent(const struct Ipc::ipc_response& res) {
  try {
    std::lock_guard<std::mutex> lock(mutex_);
    auto payload = parser_.parse(res.payload)["input"];
    if (payload["type"].asString() == "keyboard") {
        auto layout_name = payload["xkb_active_layout_name"].asString().substr(0,2);
        lang_ = Glib::Markup::escape_text(layout_name);
    }
    dp.emit();
  } catch (const std::exception& e) {
    spdlog::error("Language: {}", e.what());
  }
}

std::string Language::getIcon(const std::string& key) {
  auto format_icons = config_["format-icons"];
  if (format_icons.isObject() && format_icons[key].isString()) {
    return format_icons[key].asString();
  }
  return "";
}

auto Language::update() -> void {
  if (lang_.empty()) {
    event_box_.hide();
  } else {
    label_.set_markup(fmt::format(format_,
                                  lang_,
                                  fmt::arg("icon", getIcon(lang_))));
    if (tooltipEnabled()) {
      label_.set_tooltip_text(lang_);
    }
    event_box_.show();
  }
  // Call parent update
  ALabel::update();
}

}  // namespace waybar::modules::sway
