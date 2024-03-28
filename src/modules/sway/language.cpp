#include "modules/sway/language.hpp"

#include <fmt/core.h>
#include <json/json.h>
#include <spdlog/spdlog.h>
#include <xkbcommon/xkbregistry.h>

#include <cstring>
#include <string>
#include <vector>

#include "modules/sway/ipc/ipc.hpp"
#include "util/string.hpp"

namespace waybar::modules::sway {

const std::string Language::XKB_LAYOUT_NAMES_KEY = "xkb_layout_names";
const std::string Language::XKB_ACTIVE_LAYOUT_NAME_KEY = "xkb_active_layout_name";

Language::Language(const std::string& id, const Json::Value& config)
    : ALabel(config, "language", id, "{}", 0, true) {
  hide_single_ = config["hide-single-layout"].isBool() && config["hide-single-layout"].asBool();
  if (format_.find("{}") != std::string::npos || format_.find("{short}") != std::string::npos) {
    visible_fields |= VisibleFields::ShortName;
  }
  if (format_.find("{shortDescription}") != std::string::npos) {
    visible_fields |= VisibleFields::ShortDescription;
  }
  if (format_.find("{variant}") != std::string::npos) {
    visible_fields |= VisibleFields::Variant;
  }
  if (config.isMember("tooltip-format")) {
    tooltip_format_ = config["tooltip-format"].asString();
  }
  ipc_.subscribe(R"(["input"])");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &Language::onEvent));
  ipc_.signal_cmd.connect(sigc::mem_fun(*this, &Language::onCmd));
  ipc_.sendCmd(IPC_GET_INPUTS);
  // Launch worker
  ipc_.setWorker([this] {
    try {
      ipc_.handleEvent();
    } catch (const std::exception& e) {
      spdlog::error("Language: {}", e.what());
    }
  });
  dp.emit();
}

void Language::onCmd(const struct Ipc::ipc_response& res) {
  if (res.type != IPC_GET_INPUTS) {
    return;
  }

  try {
    std::lock_guard<std::mutex> lock(mutex_);
    auto payload = parser_.parse(res.payload);
    // Display current layout of a device with a maximum count of layouts, expecting that all will
    // be OK
    Json::ArrayIndex max_id = 0, max = 0;
    for (Json::ArrayIndex i = 0; i < payload.size(); i++) {
      auto size = payload[i][XKB_LAYOUT_NAMES_KEY].size();
      if (size > max) {
        max = size;
        max_id = i;
      }
    }

    for (const auto& layout : payload[max_id][XKB_LAYOUT_NAMES_KEY]) {
      if (std::find(layouts_.begin(), layouts_.end(), layout.asString()) == layouts_.end()) {
        layouts_.push_back(layout.asString());
      }
    }

    spdlog::debug("Language: layouts from the compositor: {}",
                  fmt::join(layouts_.begin(), layouts_.end(), ", "));

    // The configured format string does not allow distinguishing layout variants.
    // Add numeric suffixes to the layouts with the same short name.
    bool want_unique_names = ((visible_fields & VisibleFields::Variant) == 0) &&
                             ((visible_fields & ~VisibleFields::Variant) != 0);

    xkb_context_.initLayouts(layouts_, want_unique_names);
    set_current_layout(payload[max_id][XKB_ACTIVE_LAYOUT_NAME_KEY].asString());
    dp.emit();
  } catch (const std::exception& e) {
    spdlog::error("Language: {}", e.what());
  }
}

void Language::onEvent(const struct Ipc::ipc_response& res) {
  if (res.type != IPC_EVENT_INPUT) {
    return;
  }

  try {
    std::lock_guard<std::mutex> lock(mutex_);
    auto payload = parser_.parse(res.payload);

    const auto& input = payload["input"];
    if (input["type"] != "keyboard") {
      return;
    }

    const auto& change = payload["change"];
    if (change == "added" || change == "xkb_keymap") {
      // Update list of configured layouts
      for (const auto& name : input[XKB_LAYOUT_NAMES_KEY]) {
        if (std::find(layouts_.begin(), layouts_.end(), name.asString()) == layouts_.end()) {
          layouts_.push_back(name.asString());
        }
      }
    }

    const auto& layout = input[XKB_ACTIVE_LAYOUT_NAME_KEY].asString();
    spdlog::trace("Language: set layout {} for device {}", layout, input["identifier"].asString());
    set_current_layout(layout);
    dp.emit();
  } catch (const std::exception& e) {
    spdlog::error("Language: {}", e.what());
  }
}

auto Language::update() -> void {
  std::lock_guard<std::mutex> lock(mutex_);
  if (hide_single_ && layouts_.size() <= 1) {
    event_box_.hide();
    return;
  }
  auto display_layout = trim(fmt::format(
      fmt::runtime(format_), fmt::arg("short", layout_.short_name),
      fmt::arg("shortDescription", layout_.short_description), fmt::arg("long", layout_.full_name),
      fmt::arg("variant", layout_.variant), fmt::arg("flag", layout_.country_flag())));
  label_.set_markup(display_layout);
  if (tooltipEnabled()) {
    if (!tooltip_format_.empty()) {
      auto tooltip_display_layout = trim(
          fmt::format(fmt::runtime(tooltip_format_), fmt::arg("short", layout_.short_name),
                      fmt::arg("shortDescription", layout_.short_description),
                      fmt::arg("long", layout_.full_name), fmt::arg("variant", layout_.variant),
                      fmt::arg("flag", layout_.country_flag())));
      label_.set_tooltip_markup(tooltip_display_layout);
    } else {
      label_.set_tooltip_markup(display_layout);
    }
  }

  event_box_.show();

  // Call parent update
  ALabel::update();
}

auto Language::set_current_layout(std::string current_layout) -> void {
  label_.get_style_context()->remove_class(layout_.short_name);
  layout_ = xkb_context_.getLayout(current_layout);
  label_.get_style_context()->add_class(layout_.short_name);
}

const Language::Layout Language::XKBContext::fallback_;

Language::XKBContext::XKBContext() {
  context_ = rxkb_context_new(RXKB_CONTEXT_LOAD_EXOTIC_RULES);
  rxkb_context_parse_default_ruleset(context_);
}

void Language::XKBContext::initLayouts(const std::vector<std::string>& names,
                                       bool want_unique_names) {
  want_unique_names_ = want_unique_names;

  for (auto* xkb_layout = rxkb_layout_first(context_); xkb_layout != nullptr;
       xkb_layout = rxkb_layout_next(xkb_layout)) {
    const auto* short_name = rxkb_layout_get_name(xkb_layout);
    /* Assume that we see base layout first and remember it */
    if (const auto* brief = rxkb_layout_get_brief(xkb_layout); brief != nullptr) {
      base_layouts_by_name_.emplace(short_name, xkb_layout);
    }

    const auto* full_name = rxkb_layout_get_description(xkb_layout);
    if (std::find(names.begin(), names.end(), full_name) == names.end()) {
      continue;
    }

    newCachedEntry(full_name, xkb_layout);
  }
}

const Language::Layout& Language::XKBContext::getLayout(const std::string& name) {
  if (auto it = cached_layouts_.find(name); it != cached_layouts_.end()) {
    return it->second;
  }

  for (auto* xkb_layout = rxkb_layout_first(context_); xkb_layout != nullptr;
       xkb_layout = rxkb_layout_next(xkb_layout)) {
    if (name == rxkb_layout_get_description(xkb_layout)) {
      return newCachedEntry(name, xkb_layout);
    }
  }

  return fallback_;
}

Language::Layout& Language::XKBContext::newCachedEntry(const std::string& name,
                                                       rxkb_layout* xkb_layout) {
  auto [it, added] = cached_layouts_.try_emplace(name, xkb_layout);
  /* Existing entry (shouldn't happen) */
  if (!added) return it->second;

  Layout& layout = it->second;

  if (layout.short_description.empty()) {
    if (auto base = base_layouts_by_name_.find(layout.short_name);
        base != base_layouts_by_name_.end()) {
      layout.short_description = rxkb_layout_get_brief(base->second);
    }
  }

  if (want_unique_names_) {
    // Fetch and count already cached layouts with this short name
    auto [first, last] = layouts_by_short_name_.equal_range(layout.short_name);
    auto count = std::distance(first, last);
    // Add the new layout before we modify its short name
    layouts_by_short_name_.emplace(layout.short_name, layout);

    if (count > 0) {
      if (count == 1) {
        // First duplicate appeared, add suffix to the original entry
        first->second.addShortNameSuffix("1");
      }
      layout.addShortNameSuffix(std::to_string(count + 1));
    }
  }

  spdlog::debug("Language: new layout '{}' short='{}' variant='{}' shortDescription='{}'",
                layout.full_name, layout.short_name, layout.variant, layout.short_description);
  return layout;
}

Language::XKBContext::~XKBContext() { rxkb_context_unref(context_); }

Language::Layout::Layout(rxkb_layout* xkb_layout) {
  short_name = rxkb_layout_get_name(xkb_layout);
  full_name = rxkb_layout_get_description(xkb_layout);

  if (const auto* value = rxkb_layout_get_variant(xkb_layout)) {
    variant = value;
  }

  if (const auto* value = rxkb_layout_get_brief(xkb_layout)) {
    short_description = value;
  }
}

std::string Language::Layout::country_flag() const {
  if (short_name.size() != 2) return "";
  unsigned char result[] = "\xf0\x9f\x87\x00\xf0\x9f\x87\x00";
  result[3] = short_name[0] + 0x45;
  result[7] = short_name[1] + 0x45;
  // Check if both emojis are in A-Z symbol bounds
  if (result[3] < 0xa6 || result[3] > 0xbf) return "";
  if (result[7] < 0xa6 || result[7] > 0xbf) return "";
  return std::string{reinterpret_cast<char*>(result)};
}

void Language::Layout::addShortNameSuffix(std::string_view suffix) {
  short_name += suffix;
  if (!short_description.empty()) {
    short_description += suffix;
  }
}

}  // namespace waybar::modules::sway
