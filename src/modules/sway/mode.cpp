#include "modules/sway/mode.hpp"
#include <spdlog/spdlog.h>

namespace waybar::modules::sway {

Mode::Mode(const std::string& id, const Json::Value& config)
    : ALabel(config, "mode", id, "{}", "{}", 0, true) {
  ipc_.subscribe(R"(["mode"])");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &Mode::onEvent));
  // Launch worker
  ipc_.setWorker([this] {
    try {
      ipc_.handleEvent();
    } catch (const std::exception& e) {
      spdlog::error("Mode: {}", e.what());
    }
  });
  dp.emit();
}

void Mode::onEvent(const struct Ipc::ipc_response& res) {
  try {
    std::lock_guard<std::mutex> lock(mutex_);
    auto                        payload = parser_.parse(res.payload);
    if (payload["change"] != "default") {
      if (payload["pango_markup"].asBool()) {
        mode_ = payload["change"].asString();
      } else {
        mode_ = Glib::Markup::escape_text(payload["change"].asString());
      }
    } else {
      mode_.clear();
    }
    dp.emit();
  } catch (const std::exception& e) {
    spdlog::error("Mode: {}", e.what());
  }
}

auto Mode::update(std::string format, fmt::dynamic_format_arg_store<fmt::format_context>& args) -> void {
  auto modeArg = fmt::format(format_, mode_);
  args.push_back(std::cref(modeArg));
  // Call parent update
  ALabel::update(format, args);
}

}  // namespace waybar::modules::sway
