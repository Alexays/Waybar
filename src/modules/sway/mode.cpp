#include "modules/sway/mode.hpp"
#include <spdlog/spdlog.h>

namespace waybar::modules::sway {

Mode::Mode(const std::string& id, const Json::Value& config)
    : ALabel(config, "mode", id, "{}", 0, true) {
  args_.push_back(Arg{"mode", std::bind(&Mode::getMode, this), DEFAULT | TOOLTIP});
  ipc_.subscribe(R"(["mode"])");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &Mode::onEvent));
  // Launch worker
  worker();
  dp.emit();
}

void Mode::onEvent(const struct Ipc::ipc_response& res) {
  try {
    std::lock_guard<std::mutex> lock(mutex_);
    auto                        payload = parser_.parse(res.payload);
    if (payload["change"] != "default") {
      mode_ = Glib::Markup::escape_text(payload["change"].asString());
    } else {
      mode_.clear();
    }
    dp.emit();
  } catch (const std::exception& e) {
    spdlog::error("Mode: {}", e.what());
  }
}

void Mode::worker() {
  thread_ = [this] {
    try {
      ipc_.handleEvent();
    } catch (const std::exception& e) {
      spdlog::error("Mode: {}", e.what());
    }
  };
}

const std::string& Mode::getMode() const { return mode_; }

}  // namespace waybar::modules::sway
