#include "modules/sway/scratchpad.hpp"

#include <spdlog/spdlog.h>

#include <string>

namespace waybar::modules::sway {
Scratchpad::Scratchpad(const std::string& id, const Json::Value& config)
    : ALabel(config, "scratchpad", id,
             config["format"].isString() ? config["format"].asString() : "{icon} {count}"),
      tooltip_format_(config_["tooltip-format"].isString() ? config_["tooltip-format"].asString()
                                                           : "{app}: {title}"),
      show_empty_(config_["show-empty"].isBool() ? config_["show-empty"].asBool() : false),
      tooltip_enabled_(config_["tooltip"].isBool() ? config_["tooltip"].asBool() : true),
      tooltip_text_(""),
      count_(0) {
  ipc_.subscribe(R"(["window"])");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &Scratchpad::onEvent));
  ipc_.signal_cmd.connect(sigc::mem_fun(*this, &Scratchpad::onCmd));

  getTree();

  ipc_.setWorker([this] {
    try {
      ipc_.handleEvent();
    } catch (const std::exception& e) {
      spdlog::error("Scratchpad: {}", e.what());
    }
  });
}
auto Scratchpad::update() -> void {
  if (count_ || show_empty_) {
    event_box_.show();
    label_.set_markup(
        fmt::format(format_, fmt::arg("icon", getIcon(count_, "", config_["format-icons"].size())),
                    fmt::arg("count", count_)));
    if (tooltip_enabled_) {
      label_.set_tooltip_markup(tooltip_text_);
    }
  } else {
    event_box_.hide();
  }
  if (count_) {
    label_.get_style_context()->remove_class("empty");
  } else {
    label_.get_style_context()->add_class("empty");
  }
  ALabel::update();
}

auto Scratchpad::getTree() -> void {
  try {
    ipc_.sendCmd(IPC_GET_TREE);
  } catch (const std::exception& e) {
    spdlog::error("Scratchpad: {}", e.what());
  }
}

auto Scratchpad::onCmd(const struct Ipc::ipc_response& res) -> void {
  try {
    std::lock_guard<std::mutex> lock(mutex_);
    auto tree = parser_.parse(res.payload);
    count_ = tree["nodes"][0]["nodes"][0]["floating_nodes"].size();
    if (tooltip_enabled_) {
      tooltip_text_.clear();
      for (const auto& window : tree["nodes"][0]["nodes"][0]["floating_nodes"]) {
        tooltip_text_.append(fmt::format(tooltip_format_ + '\n',
                                         fmt::arg("app", window["app_id"].asString()),
                                         fmt::arg("title", window["name"].asString())));
      }
      if (!tooltip_text_.empty()) {
        tooltip_text_.pop_back();
      }
    }
    dp.emit();
  } catch (const std::exception& e) {
    spdlog::error("Scratchpad: {}", e.what());
  }
}

auto Scratchpad::onEvent(const struct Ipc::ipc_response& res) -> void { getTree(); }
}  // namespace waybar::modules::sway