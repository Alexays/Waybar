#include "modules/hyprland/language.hpp"

#include <spdlog/spdlog.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbregistry.h>

#include <util/sanitize_str.hpp>

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
  gIPC->registerForIPC("activelayout", this);
}

Language::~Language() {
  gIPC->unregisterForIPC(this);
  // wait for possible event handler to finish
  std::lock_guard<std::mutex> lg(mutex_);
}

auto Language::update() -> void {
  std::lock_guard<std::mutex> lg(mutex_);

  if (!format_.empty()) {
    label_.show();
    label_.set_markup(layoutName_);
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

  const auto BRIEFNAME = getShortFrom(layoutName);

  if (config_.isMember("format-" + BRIEFNAME)) {
    const auto PROPNAME = "format-" + BRIEFNAME;
    layoutName = fmt::format(format_, config_[PROPNAME].asString());
  } else {
    layoutName = fmt::format(format_, layoutName);
  }

  layoutName = waybar::util::sanitize_string(layoutName);

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

std::string Language::getShortFrom(const std::string& fullName) {
  const auto CONTEXT = rxkb_context_new(RXKB_CONTEXT_LOAD_EXOTIC_RULES);
  rxkb_context_parse_default_ruleset(CONTEXT);

  std::string foundName = "";
  rxkb_layout* layout = rxkb_layout_first(CONTEXT);
  while (layout) {
    std::string nameOfLayout = rxkb_layout_get_description(layout);

    if (nameOfLayout != fullName) {
      layout = rxkb_layout_next(layout);
      continue;
    }

    std::string briefName = rxkb_layout_get_brief(layout);

    rxkb_context_unref(CONTEXT);

    return briefName;
  }

  rxkb_context_unref(CONTEXT);

  return "";
}

}  // namespace waybar::modules::hyprland
