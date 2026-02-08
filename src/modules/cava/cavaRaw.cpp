#include "modules/cava/cavaRaw.hpp"

#include <spdlog/spdlog.h>

waybar::modules::cava::Cava::Cava(const std::string& id, const Json::Value& config,
                                  std::mutex& reap_mtx, std::list<pid_t>& reap)
    : ALabel(config, "cava", id, "{}", reap_mtx, reap, 60, false, false, false),
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

// Cava actions
void waybar::modules::cava::Cava::pause_resume() { backend_->doPauseResume(); }
auto waybar::modules::cava::Cava::onUpdate(const std::string& input) -> void {
  Glib::signal_idle().connect_once([this, input]() {
    if (silence_) {
      label_.get_style_context()->remove_class("silent");
      if (!label_.get_style_context()->has_class("updated"))
        label_.get_style_context()->add_class("updated");
    }
    label_text_.clear();
    for (auto& ch : input)
      label_text_.append(getIcon((ch > ascii_range_) ? ascii_range_ : ch, "", ascii_range_ + 1));

    label_.set_markup(label_text_);
    label_.show();
    ALabel::update();
  });
  silence_ = false;
}

auto waybar::modules::cava::Cava::onSilence() -> void {
  Glib::signal_idle().connect_once([this]() {
    if (!silence_) {
      if (label_.get_style_context()->has_class("updated"))
        label_.get_style_context()->remove_class("updated");

      if (hide_on_silence_) {
        // Clear the label markup before hiding to prevent GTK from rendering a NULL Pango layout
        label_.set_markup("");
        label_.hide();
      } else if (config_["format_silent"].isString())
        label_.set_markup(format_silent_);
      silence_ = true;
      label_.get_style_context()->add_class("silent");
    }
  });
}
