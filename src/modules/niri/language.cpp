#include "modules/niri/language.hpp"

#include <spdlog/spdlog.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbregistry.h>

#include "util/string.hpp"

namespace waybar::modules::niri {

Language::Language(const std::string &id, const Bar &bar, const Json::Value &config)
    : ALabel(config, "language", id, "{}", 0, false), bar_(bar) {
  label_.hide();

  if (!gIPC) gIPC = std::make_unique<IPC>();

  gIPC->registerForIPC("KeyboardLayoutsChanged", this);
  gIPC->registerForIPC("KeyboardLayoutSwitched", this);

  updateFromIPC();
  dp.emit();
}

Language::~Language() {
  gIPC->unregisterForIPC(this);
  // wait for possible event handler to finish
  std::lock_guard<std::mutex> lock(mutex_);
}

void Language::updateFromIPC() {
  std::lock_guard<std::mutex> lock(mutex_);
  auto ipcLock = gIPC->lockData();

  layouts_.clear();
  for (const auto &fullName : gIPC->keyboardLayoutNames()) layouts_.push_back(getLayout(fullName));

  current_idx_ = gIPC->keyboardLayoutCurrent();
}

/**
 *  Language::doUpdate - update workspaces in UI thread.
 *
 * Note: some member fields are modified by both UI thread and event listener thread, use mutex_ to
 *       protect these member fields, and lock should released before calling ALabel::update().
 */
void Language::doUpdate() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (layouts_.size() <= current_idx_) {
    spdlog::error("niri language layout index out of bounds");
    label_.hide();
    return;
  }
  const auto &layout = layouts_[current_idx_];

  spdlog::debug("niri language update with full name {}", layout.full_name);
  spdlog::debug("niri language update with short name {}", layout.short_name);
  spdlog::debug("niri language update with short description {}", layout.short_description);
  spdlog::debug("niri language update with variant {}", layout.variant);

  std::string layoutName = std::string{};
  if (config_.isMember("format-" + layout.short_description + "-" + layout.variant)) {
    const auto propName = "format-" + layout.short_description + "-" + layout.variant;
    layoutName = fmt::format(fmt::runtime(format_), config_[propName].asString());
  } else if (config_.isMember("format-" + layout.short_description)) {
    const auto propName = "format-" + layout.short_description;
    layoutName = fmt::format(fmt::runtime(format_), config_[propName].asString());
  } else {
    layoutName = trim(fmt::format(fmt::runtime(format_), fmt::arg("long", layout.full_name),
                                  fmt::arg("short", layout.short_name),
                                  fmt::arg("shortDescription", layout.short_description),
                                  fmt::arg("variant", layout.variant)));
  }

  spdlog::debug("niri language formatted layout name {}", layoutName);

  if (!format_.empty()) {
    label_.show();
    label_.set_markup(layoutName);
  } else {
    label_.hide();
  }
}

void Language::update() {
  doUpdate();
  ALabel::update();
}

void Language::onEvent(const Json::Value &ev) {
  if (ev["KeyboardLayoutsChanged"]) {
    updateFromIPC();
  } else if (ev["KeyboardLayoutSwitched"]) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto ipcLock = gIPC->lockData();
    current_idx_ = gIPC->keyboardLayoutCurrent();
  }

  dp.emit();
}

Language::Layout Language::getLayout(const std::string &fullName) {
  auto *const context = rxkb_context_new(RXKB_CONTEXT_LOAD_EXOTIC_RULES);
  rxkb_context_parse_default_ruleset(context);

  rxkb_layout *layout = rxkb_layout_first(context);
  while (layout != nullptr) {
    std::string nameOfLayout = rxkb_layout_get_description(layout);

    if (nameOfLayout != fullName) {
      layout = rxkb_layout_next(layout);
      continue;
    }

    auto name = std::string(rxkb_layout_get_name(layout));
    const auto *variantPtr = rxkb_layout_get_variant(layout);
    std::string variant = variantPtr == nullptr ? "" : std::string(variantPtr);

    const auto *descriptionPtr = rxkb_layout_get_brief(layout);
    std::string description = descriptionPtr == nullptr ? "" : std::string(descriptionPtr);

    Layout info = Layout{nameOfLayout, name, variant, description};

    rxkb_context_unref(context);

    return info;
  }

  rxkb_context_unref(context);

  spdlog::debug("niri language didn't find matching layout for {}", fullName);

  return Layout{"", "", "", ""};
}

}  // namespace waybar::modules::niri
