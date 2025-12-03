#include "modules/cava/cava.hpp"

#include <glibmm/main.h>
#include <spdlog/spdlog.h>

waybar::modules::cava::Cava::Cava(const std::string& id, const Json::Value& config)
    : ALabel(config, "cava", id, "{}", 60, false, false, false),
      backend_{waybar::modules::cava::CavaBackend::inst(config)} {
  if (config_["hide_on_silence"].isBool()) hide_on_silence_ = config_["hide_on_silence"].asBool();
  if (config_["format_silent"].isString()) format_silent_ = config_["format_silent"].asString();

  ascii_range_ = backend_->getAsciiRange();
  backend_->signal_update().connect(sigc::mem_fun(*this, &Cava::onUpdate));
  backend_->signal_silence().connect(sigc::mem_fun(*this, &Cava::onSilence));
  backend_->Update();
}

auto waybar::modules::cava::Cava::doAction(const std::string& name) -> void {
  if ((actionMap_[name])) {
    (this->*actionMap_[name])();
  } else
    spdlog::error("Cava. Unsupported action \"{0}\"", name);
}

void waybar::modules::cava::Cava::pause_resume() { backend_->doPauseResume(); }

auto waybar::modules::cava::Cava::onUpdate(const std::string& input) -> void {
  silence_ = false;
  Glib::signal_idle().connect_once([this, input]() {
    auto ctx = label_.get_style_context();
    if (ctx->has_class("silent")) {
      ctx->remove_class("silent");
      ctx->add_class("updated");
    }
    label_text_.clear();
    for (const auto& ch : input)
      label_text_.append(getIcon((ch > ascii_range_) ? ascii_range_ : ch, "", ascii_range_ + 1));
    label_.set_markup(label_text_);
    label_.show();
    ALabel::update();
  });
}

auto waybar::modules::cava::Cava::onSilence() -> void {
  if (silence_) return;
  silence_ = true;
  Glib::signal_idle().connect_once([this]() {
    auto ctx = label_.get_style_context();
    if (ctx->has_class("updated")) ctx->remove_class("updated");
    if (hide_on_silence_)
      label_.hide();
    else if (!format_silent_.empty())
      label_.set_markup(format_silent_);
    ctx->add_class("silent");
  });
}
