#include "modules/mango/language.hpp"

#include <spdlog/spdlog.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbregistry.h>

#include "util/string.hpp"

namespace waybar::modules::mango {

Language::Language(const std::string& id, const Bar& bar, const Json::Value& config)
    : ALabel(config, "language", id, "{}", 0, false), bar_(bar), rxkb_ctx_(nullptr) {
  rxkb_ctx_ = rxkb_context_new(RXKB_CONTEXT_LOAD_EXOTIC_RULES);
  if (rxkb_ctx_) {
    rxkb_context_parse_default_ruleset(rxkb_ctx_);
  }

  IPC::getInstance().registerForIPC("monitor", this);
  updateFromIPC();
}

Language::~Language() {
  IPC::getInstance().unregisterForIPC(this);
  if (rxkb_ctx_) rxkb_context_unref(rxkb_ctx_);
}

void Language::updateFromIPC() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string layout = IPC::getInstance().getKeyboardLayout();

  layouts_.clear();
  if (!layout.empty()) {
    Layout l = getLayout(layout);
    layouts_.push_back(l);
    current_idx_ = 0;
  } else {
    current_idx_ = 0;
  }
}

void Language::doUpdate() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (layouts_.empty() || current_idx_ >= layouts_.size()) {
    label_.hide();
    return;
  }
  const auto& layout = layouts_[current_idx_];

  if (!last_short_name_.empty()) label_.get_style_context()->remove_class(last_short_name_);
  if (!layout.short_name.empty()) {
    label_.get_style_context()->add_class(layout.short_name);
    last_short_name_ = layout.short_name;
  }

  std::string layoutName;
  std::string variant_key = "format-" + layout.short_description + "-" + layout.variant;
  if (!layout.variant.empty() && config_.isMember(variant_key)) {
    layoutName =
        fmt::format(fmt::runtime(config_[variant_key].asString()),
                    fmt::arg("long", layout.full_name), fmt::arg("short", layout.short_name),
                    fmt::arg("shortDescription", layout.short_description),
                    fmt::arg("variant", layout.variant));
  } else if (config_.isMember("format-" + layout.short_description)) {
    std::string key = "format-" + layout.short_description;
    layoutName =
        fmt::format(fmt::runtime(config_[key].asString()), fmt::arg("long", layout.full_name),
                    fmt::arg("short", layout.short_name),
                    fmt::arg("shortDescription", layout.short_description),
                    fmt::arg("variant", layout.variant));
  } else {
    layoutName = fmt::format(fmt::runtime(format_), fmt::arg("long", layout.full_name),
                             fmt::arg("short", layout.short_name),
                             fmt::arg("shortDescription", layout.short_description),
                             fmt::arg("variant", layout.variant));
  }

  if (!layoutName.empty()) {
    label_.show();
    label_.set_markup(layoutName);
  } else {
    label_.hide();
  }
}

void Language::update() {
  updateFromIPC();
  doUpdate();
  ALabel::update();
}

void Language::onEvent(const Json::Value& ev) {
  updateFromIPC();
  dp.emit();
}

Language::Layout Language::getLayout(const std::string& fullName) {
  if (rxkb_ctx_) {
    rxkb_layout* layout = rxkb_layout_first(rxkb_ctx_);
    while (layout != nullptr) {
      std::string desc = rxkb_layout_get_description(layout);
      if (desc == fullName) {
        std::string short_name = rxkb_layout_get_name(layout);
        const char* variant_ptr = rxkb_layout_get_variant(layout);
        std::string variant = variant_ptr ? variant_ptr : "";
        const char* brief_ptr = rxkb_layout_get_brief(layout);
        std::string short_description = brief_ptr ? brief_ptr : "";

        if (short_description.empty()) {
          short_description = short_name;
        }

        Layout info{desc, short_name, variant, short_description};
        return info;
      }
      layout = rxkb_layout_next(layout);
    }
  }

  spdlog::warn("mango language: rxkb failed to find layout '{}', using string parsing fallback",
               fullName);

  Layout l;
  l.full_name = fullName;
  l.variant = "";

  size_t paren_start = fullName.find('(');
  size_t paren_end = fullName.find(')');
  if (paren_start != std::string::npos && paren_end != std::string::npos &&
      paren_end > paren_start) {
    l.short_name = fullName.substr(paren_start + 1, paren_end - paren_start - 1);
  } else if (fullName.length() >= 2) {
    l.short_name = fullName.substr(0, 2);
  } else {
    l.short_name = fullName;
  }

  std::transform(l.short_name.begin(), l.short_name.end(), l.short_name.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  l.short_description = l.short_name;

  return l;
}

}  // namespace waybar::modules::mango