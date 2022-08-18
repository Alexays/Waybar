#include "modules/hyprland/language.hpp"

#include <spdlog/spdlog.h>

#include "modules/hyprland/backend.hpp"

namespace waybar::modules::hyprland {

Language::Language(const std::string& id, const Bar& bar, const Json::Value& config)
    : ALabel(config, "language", id, "{}", 0, true), bar_(bar) {
  modulesReady = true;

  if (!gIPC.get()) {
    gIPC = std::make_unique<IPC>();
  }

  // get the active layout when open
  initLanguage();

  label_.hide();
  ALabel::update();

  // register for hyprland ipc
  gIPC->registerForIPC("activelayout", [&](const std::string& ev) { this->onEvent(ev); });
}

auto Language::update() -> void {
  // fix ampersands
  std::lock_guard<std::mutex> lg(mutex_);

  if (!format_.empty()) {
    label_.show();
    label_.set_markup(fmt::format(format_, layoutName_));
  } else {
    label_.hide();
  }

  ALabel::update();
}

void Language::onEvent(const std::string& ev) {
  std::lock_guard<std::mutex> lg(mutex_);
  auto layoutName = ev.substr(ev.find_last_of(',') + 1);
  auto keebName = ev.substr(0, ev.find_last_of(','));
  keebName = keebName.substr(keebName.find_first_of('>') + 2);

  if (config_.isMember("keyboard-name") && keebName != config_["keyboard-name"].asString())
    return;  // ignore

  auto replaceAll = [](std::string str, const std::string& from,
                       const std::string& to) -> std::string {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
      str.replace(start_pos, from.length(), to);
      start_pos += to.length();
    }
    return str;
  };

  layoutName = replaceAll(layoutName, "&", "&amp;");

  if (layoutName == layoutName_) return;

  layoutName_ = layoutName;

  spdlog::debug("hyprland language onevent with {}", layoutName);

  dp.emit();
}

void Language::initLanguage() {
  const auto INPUTDEVICES = gIPC->getSocket1Reply("devices");

  if (!config_.isMember("keyboard-name")) return;

  const auto KEEBNAME = config_["keyboard-name"].asString();

  try {
    auto searcher = INPUTDEVICES.substr(INPUTDEVICES.find(KEEBNAME) + KEEBNAME.length());
    searcher = searcher.substr(searcher.find("Keyboard at"));
    searcher = searcher.substr(searcher.find("keymap:") + 7);
    searcher = searcher.substr(0, searcher.find_first_of("\n\t"));

    layoutName_ = searcher;

    spdlog::debug("hyprland language initLanguage found {}", layoutName_);

    dp.emit();

  } catch (std::exception& e) {
    spdlog::error("hyprland language initLanguage failed with {}", e.what());
  }
}

}  // namespace waybar::modules::hyprland