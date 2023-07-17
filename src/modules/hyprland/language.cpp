#include "modules/hyprland/language.hpp"

#include <spdlog/spdlog.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbregistry.h>

#include <util/sanitize_str.hpp>

#include "util/string.hpp"

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
  update();

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

  std::string layoutName = std::string{};
  if (config_.isMember("format-" + layout_.short_description + "-" + layout_.variant)) {
    const auto propName = "format-" + layout_.short_description + "-" + layout_.variant;
    layoutName = fmt::format(fmt::runtime(format_), config_[propName].asString());
  } else if (config_.isMember("format-" + layout_.short_description)) {
    const auto propName = "format-" + layout_.short_description;
    layoutName = fmt::format(fmt::runtime(format_), config_[propName].asString());
  } else {
    layoutName = trim(fmt::format(fmt::runtime(format_), fmt::arg("long", layout_.full_name),
                                  fmt::arg("short", layout_.short_name),
                                  fmt::arg("shortDescription", layout_.short_description),
                                  fmt::arg("variant", layout_.variant)));
  }

  if (!format_.empty()) {
    label_.show();
    label_.set_markup(layoutName);
  } else {
    label_.hide();
  }

  ALabel::update();
}

void Language::onEvent(const std::string& ev) {
  std::lock_guard<std::mutex> lg(mutex_);
  std::string kbName(begin(ev) + ev.find_last_of('>') + 1, begin(ev) + ev.find_first_of(','));
  auto layoutName = ev.substr(ev.find_first_of(',') + 1);

  if (config_.isMember("keyboard-name") && kbName != config_["keyboard-name"].asString())
    return;  // ignore

  layoutName = waybar::util::sanitize_string(layoutName);

  layout_ = getLayout(layoutName);

  spdlog::debug("hyprland language onevent with {}", layoutName);

  dp.emit();
}

void Language::initLanguage() {
  const auto inputDevices = gIPC->getSocket1Reply("devices");

  const auto kbName = config_["keyboard-name"].asString();

  try {
    auto searcher = kbName.empty()
                        ? inputDevices
                        : inputDevices.substr(inputDevices.find(kbName) + kbName.length());
    searcher = searcher.substr(searcher.find("keymap:") + 8);
    searcher = searcher.substr(0, searcher.find_first_of("\n\t"));

    searcher = waybar::util::sanitize_string(searcher);

    layout_ = getLayout(searcher);

    spdlog::debug("hyprland language initLanguage found {}", layout_.full_name);

    dp.emit();

  } catch (std::exception& e) {
    spdlog::error("hyprland language initLanguage failed with {}", e.what());
  }
}

auto Language::getLayout(const std::string& fullName) -> Layout {
  const auto CONTEXT = rxkb_context_new(RXKB_CONTEXT_LOAD_EXOTIC_RULES);
  rxkb_context_parse_default_ruleset(CONTEXT);

  rxkb_layout* layout = rxkb_layout_first(CONTEXT);
  while (layout) {
    std::string nameOfLayout = rxkb_layout_get_description(layout);

    if (nameOfLayout != fullName) {
      layout = rxkb_layout_next(layout);
      continue;
    }

    auto name = std::string(rxkb_layout_get_name(layout));
    auto variant_ = rxkb_layout_get_variant(layout);
    std::string variant = variant_ == nullptr ? "" : std::string(variant_);

    auto short_description_ = rxkb_layout_get_brief(layout);
    std::string short_description =
        short_description_ == nullptr ? "" : std::string(short_description_);

    Layout info = Layout{nameOfLayout, name, variant, short_description};

    rxkb_context_unref(CONTEXT);

    return info;
  }

  rxkb_context_unref(CONTEXT);

  spdlog::debug("hyprland language didn't find matching layout");

  return Layout{"", "", "", ""};
}

}  // namespace waybar::modules::hyprland
