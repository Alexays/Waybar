#include "modules/hyprland/language.hpp"

#include <spdlog/spdlog.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbregistry.h>

#include "util/sanitize_str.hpp"
#include "util/string.hpp"

namespace waybar::modules::hyprland {

Language::Language(const std::string& id, const Bar& bar, const Json::Value& config)
    : ALabel(config, "language", id, "{}", 0, true), bar_(bar), m_ipc(IPC::inst()) {
  modulesReady = true;

  // get the active layout when open
  initLanguage();

  label_.hide();
  update();

  // register for hyprland ipc
  m_ipc.registerForIPC("activelayout", this);
}

Language::~Language() {
  m_ipc.unregisterForIPC(this);
  // wait for possible event handler to finish
  std::lock_guard<std::mutex> lg(mutex_);
}

auto Language::update() -> void {
  std::lock_guard<std::mutex> lg(mutex_);

  spdlog::debug("hyprland language update with full name {}", layout_.full_name);
  spdlog::debug("hyprland language update with short name {}", layout_.short_name);
  spdlog::debug("hyprland language update with short description {}", layout_.short_description);
  spdlog::debug("hyprland language update with variant {}", layout_.variant);

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

  spdlog::debug("hyprland language formatted layout name {}", layoutName);

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

  const static std::string eventPrefix("activelayout>>");

  if (!ev.starts_with(eventPrefix))
    return;

  std::string kbName;
  std::string layoutName;
  Layout layout;

  // The format of this event data is "activelayout>>KEYBOARDNAME,LAYOUTNAME",
  // but both KEYBOARDNAME and LAYOUTNAME may contain embedded commas. Try to
  // split the string at each comma until we find either the configured
  // keyboard-name or a recognized layout name.
  for (size_t i = eventPrefix.length(); (i = ev.find_first_of(',', i)) != ev.npos; i++) {
    kbName = ev.substr(eventPrefix.length(), i - eventPrefix.length());
    layoutName = waybar::util::sanitize_string(ev.substr(i + 1));
    layout = getLayout(layoutName);

    if (config_.isMember("keyboard-name") && kbName == config_["keyboard-name"].asString())
      break;

    if (layout.full_name != "")
      break;
  }

  if (config_.isMember("keyboard-name") && kbName != config_["keyboard-name"].asString())
    return;  // ignore

  layout_ = layout;

  spdlog::debug("hyprland language onevent with {}", layoutName);

  dp.emit();
}

void Language::initLanguage() {
  const auto inputDevices = m_ipc.getSocket1Reply("devices");

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
  auto* const context = rxkb_context_new(RXKB_CONTEXT_LOAD_EXOTIC_RULES);
  rxkb_context_parse_default_ruleset(context);

  rxkb_layout* layout = rxkb_layout_first(context);
  while (layout != nullptr) {
    std::string nameOfLayout = rxkb_layout_get_description(layout);

    if (nameOfLayout != fullName) {
      layout = rxkb_layout_next(layout);
      continue;
    }

    auto name = std::string(rxkb_layout_get_name(layout));
    const auto* variantPtr = rxkb_layout_get_variant(layout);
    std::string variant = variantPtr == nullptr ? "" : std::string(variantPtr);

    const auto* descriptionPtr = rxkb_layout_get_brief(layout);
    std::string description = descriptionPtr == nullptr ? "" : std::string(descriptionPtr);

    Layout info = Layout{nameOfLayout, name, variant, description};

    rxkb_context_unref(context);

    return info;
  }

  rxkb_context_unref(context);

  spdlog::debug("hyprland language didn't find matching layout");

  return Layout{"", "", "", ""};
}

}  // namespace waybar::modules::hyprland
