#include "modules/sway/mode.hpp"

namespace waybar::modules::sway {

Mode::Mode(const std::string& id, const Bar& bar, const Json::Value& config)
    : ALabel(config, "{}"), bar_(bar) {
  label_.set_name("mode");
  if (!id.empty()) {
    label_.get_style_context()->add_class(id);
  }
  ipc_.subscribe("[ \"mode\" ]");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &Mode::onEvent));
  // Launch worker
  worker();
  dp.emit();
}

void Mode::onEvent(const struct Ipc::ipc_response res) {
  if (res.payload["change"] != "default") {
    mode_ = res.payload["change"].asString();
  } else {
    mode_.clear();
  }
  dp.emit();
}

void Mode::worker() {
  thread_ = [this] {
    try {
      ipc_.handleEvent();
    } catch (const std::exception& e) {
      std::cerr << "Mode: " << e.what() << std::endl;
    }
  };
}

auto Mode::update() -> void {
  if (mode_.empty()) {
    event_box_.hide();
  } else {
    label_.set_markup(fmt::format(format_, mode_));
    if (tooltipEnabled()) {
      label_.set_tooltip_text(mode_);
    }
    event_box_.show();
  }
}

}  // namespace waybar::modules::sway