#include "modules/cava/cavaRaw.hpp"

#include <spdlog/spdlog.h>

const std::map<std::string, waybar::modules::cava::CavaRaw::Action>
    waybar::modules::cava::CavaRaw::actionMap_{{"mode", &CavaRaw::pauseResume}};

waybar::modules::cava::CavaRaw::CavaRaw(const std::string& id, const Json::Value& config,
                                        std::mutex& reap_mtx, std::list<pid_t>& reap)
    : ALabel(config, "cava", id, "{}", reap_mtx, reap, 60, false, false, false),
      backend_{waybar::modules::cava::CavaBackend::inst(config)} {
  if (config_["hide_on_silence"].isBool()) hide_on_silence_ = config_["hide_on_silence"].asBool();
  if (config_["format_silent"].isString()) format_silent_ = config_["format_silent"].asString();

  update_conn_ = backend_->signalUpdate().connect(sigc::mem_fun(*this, &CavaRaw::onUpdate));
  silence_conn_ = backend_->signalSilence().connect(sigc::mem_fun(*this, &CavaRaw::onSilence));
  backend_->update();
}

waybar::modules::cava::CavaRaw::~CavaRaw() {
  update_conn_.disconnect();
  silence_conn_.disconnect();
}

auto waybar::modules::cava::CavaRaw::doAction(const std::string& name) -> void {
  auto it = actionMap_.find(name);
  if (it != actionMap_.end() && it->second) {
    (this->*it->second)();
  } else {
    spdlog::error("Cava. Unsupported action \"{0}\"", name);
  }
}

// Cava actions
void waybar::modules::cava::CavaRaw::pauseResume() { backend_->doPauseResume(); }
auto waybar::modules::cava::CavaRaw::onUpdate(const std::string& input) -> void {
  if (silence_) {
    silence_ = false;
    label_.get_style_context()->remove_class("silent");
    if (!label_.get_style_context()->has_class("updated"))
      label_.get_style_context()->add_class("updated");
  }
  label_text_.clear();
  auto ascii_range = backend_->getAsciiRange();
  for (auto& ch : input) {
    auto uch = static_cast<unsigned char>(ch);
    label_text_.append(getIcon((uch > ascii_range) ? ascii_range : uch, "", ascii_range + 1));
  }

  label_.set_markup(label_text_);
  label_.show();
  ALabel::update();
}

auto waybar::modules::cava::CavaRaw::onSilence() -> void {
  if (!silence_) {
    if (label_.get_style_context()->has_class("updated"))
      label_.get_style_context()->remove_class("updated");

    if (hide_on_silence_) {
      // Clear the label markup before hiding to prevent GTK from rendering a NULL Pango layout
      label_.set_markup("");
      label_.hide();
    } else if (!format_silent_.empty()) {
      label_.set_markup(format_silent_);
    }
    silence_ = true;
    label_.get_style_context()->add_class("silent");
  }
}
