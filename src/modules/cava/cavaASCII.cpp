#include "modules/cava/cavaASCII.hpp"

#include <spdlog/spdlog.h>

namespace waybar::modules::cava {

const std::map<std::string, CavaASCII::Action> CavaASCII::actionMap_{
    {"mode", &CavaASCII::pauseResume}};

CavaASCII::CavaASCII(const std::string& id, const Json::Value& config)
    : ALabel(config, "cava", id, "{}", 60, false, false, false),
      backend_{CavaBackend::inst(config)} {
  if (config_["hide_on_silence"].isBool()) hide_on_silence_ = config_["hide_on_silence"].asBool();
  if (config_["format_silent"].isString()) format_silent_ = config_["format_silent"].asString();

  update_conn_ = backend_->signalUpdate().connect(sigc::mem_fun(*this, &CavaASCII::onUpdate));
  silence_conn_ = backend_->signalSilence().connect(sigc::mem_fun(*this, &CavaASCII::onSilence));
  backend_->update();
}

CavaASCII::~CavaASCII() {
  update_conn_.disconnect();
  silence_conn_.disconnect();
}

auto CavaASCII::doAction(const std::string& name) -> void {
  auto it = actionMap_.find(name);
  if (it != actionMap_.end() && it->second) {
    (this->*it->second)();
  } else {
    spdlog::error("Cava. Unsupported action \"{0}\"", name);
  }
}

// Cava actions
void CavaASCII::pauseResume() { backend_->doPauseResume(); }
auto CavaASCII::onUpdate(const std::string& input) -> void {
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
  ALabel::doUpdate();
}

auto CavaASCII::onSilence() -> void {
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

}  // namespace waybar::modules::cava
