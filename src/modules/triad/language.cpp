#include "modules/triad/language.hpp"

#include <spdlog/spdlog.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbregistry.h>

#include "util/string.hpp"

namespace waybar::modules::triad {

Language::Language(const std::string& id, const Bar& bar, const Json::Value& config)
    : ALabel(config, "language", id, "{}", 0, false), bar_(bar) {
  label_.hide();

  if (!gIPC) gIPC = std::make_unique<IPC>();

  gIPC->registerForIPC("state-changed", this);

  updateFromIPC();
  dp.emit();
}

Language::~Language() {
  gIPC->unregisterForIPC(this);
  std::lock_guard<std::mutex> lock(mutex_);
}

void Language::updateFromIPC() {
  std::lock_guard<std::mutex> lock(mutex_);
  auto ipcLock = gIPC->lockData();

  layouts_.clear();
  layouts_.reserve(gIPC->keyboardLayoutNames().size());
  for (const auto& fullName : gIPC->keyboardLayoutNames()) layouts_.push_back(getLayout(fullName));

  current_idx_ = gIPC->keyboardLayoutCurrent();
}

void Language::doUpdate() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (layouts_.size() <= current_idx_) {
    spdlog::error("triad language layout index out of bounds");
    label_.hide();
    return;
  }
  const auto& layout = layouts_[current_idx_];

  if (!last_short_name_.empty()) {
    label_.get_style_context()->remove_class(last_short_name_);
  }
  if (!layout.short_name.empty()) {
    label_.get_style_context()->add_class(layout.short_name);
    last_short_name_ = layout.short_name;
  } else {
    last_short_name_.clear();
  }

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

void Language::onEvent(const Json::Value& ev) {
  updateFromIPC();
  dp.emit();
}

Language::Layout Language::getLayout(const std::string& fullName) {
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

  spdlog::debug("triad language didn't find matching layout for {}", fullName);

  return Layout{fullName, fullName, "", fullName};
}

}  // namespace waybar::modules::triad
