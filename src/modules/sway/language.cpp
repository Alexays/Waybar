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
    std::vector<std::string> used_layouts;
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
      used_layouts.push_back(layout.asString());
    }

    init_layouts_map(used_layouts);
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
    auto payload = parser_.parse(res.payload)["input"];
    if (payload["type"].asString() == "keyboard") {
      set_current_layout(payload[XKB_ACTIVE_LAYOUT_NAME_KEY].asString());
    }
    dp.emit();
  } catch (const std::exception& e) {
    spdlog::error("Language: {}", e.what());
  }
}

auto Language::update() -> void {
  std::lock_guard<std::mutex> lock(mutex_);
  if (hide_single_ && layouts_map_.size() <= 1) {
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
  layout_ = layouts_map_[current_layout];
  label_.get_style_context()->add_class(layout_.short_name);
}

auto Language::init_layouts_map(const std::vector<std::string>& used_layouts) -> void {
  // First layout entry with this short name
  std::map<std::string_view, Layout&> layout_by_short_names;
  // Current number of layout entries with this short name
  std::map<std::string_view, int> count_by_short_names;
  XKBContext xkb_context;

  bool want_unique_names = ((visible_fields & VisibleFields::Variant) == 0) &&
                           ((visible_fields & ~VisibleFields::Variant) != 0);

  auto* layout = xkb_context.next_layout();
  for (; layout != nullptr; layout = xkb_context.next_layout()) {
    if (std::find(used_layouts.begin(), used_layouts.end(), layout->full_name) ==
        used_layouts.end()) {
      continue;
    }

    auto [it, added] = layouts_map_.emplace(layout->full_name, *layout);
    if (!added) continue;

    if (want_unique_names) {
      auto prev = layout_by_short_names.emplace(it->second.short_name, it->second).first;
      switch (int number = ++count_by_short_names[it->second.short_name]; number) {
        case 1:
          break;
        case 2:
          // First duplicate appeared, add suffix to the original entry
          prev->second.addShortNameSuffix("1");
          G_GNUC_FALLTHROUGH;
        default:
          it->second.addShortNameSuffix(std::to_string(number));
          break;
      }
    }

    spdlog::debug("Language: new layout '{}' short='{}' variant='{}' shortDescription='{}'",
                  it->second.full_name, it->second.short_name, it->second.variant,
                  it->second.short_description);
  }
}

Language::XKBContext::XKBContext() {
  context_ = rxkb_context_new(RXKB_CONTEXT_LOAD_EXOTIC_RULES);
  rxkb_context_parse_default_ruleset(context_);
}

auto Language::XKBContext::next_layout() -> Layout* {
  if (xkb_layout_ == nullptr) {
    xkb_layout_ = rxkb_layout_first(context_);
  } else {
    xkb_layout_ = rxkb_layout_next(xkb_layout_);
  }

  if (xkb_layout_ == nullptr) {
    return nullptr;
  }

  layout_ = std::make_unique<Layout>(xkb_layout_);

  if (!layout_->short_description.empty()) {
    base_layouts_by_name_.emplace(layout_->short_name, xkb_layout_);
  } else if (auto base = base_layouts_by_name_.find(layout_->short_name);
             base != base_layouts_by_name_.end()) {
    layout_->short_description = rxkb_layout_get_brief(base->second);
  }

  return layout_.get();
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
