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
  is_variant_displayed = format_.find("{variant}") != std::string::npos;
  if (format_.find("{}") != std::string::npos || format_.find("{short}") != std::string::npos) {
    displayed_short_flag |= static_cast<std::byte>(DispayedShortFlag::ShortName);
  }
  if (format_.find("{shortDescription}") != std::string::npos) {
    displayed_short_flag |= static_cast<std::byte>(DispayedShortFlag::ShortDescription);
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
  auto display_layout = trim(fmt::format(
      fmt::runtime(format_), fmt::arg("short", layout_.short_name),
      fmt::arg("shortDescription", layout_.short_description), fmt::arg("long", layout_.full_name),
      fmt::arg("variant", layout_.variant), fmt::arg("flag", layout_.country_flag())));
  label_.set_markup(display_layout);
  if (tooltipEnabled()) {
    if (tooltip_format_ != "") {
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
  std::map<std::string, std::vector<Layout*>> found_by_short_names;
  XKBContext xkb_context;
  auto layout = xkb_context.next_layout();
  for (; layout != nullptr; layout = xkb_context.next_layout()) {
    if (std::find(used_layouts.begin(), used_layouts.end(), layout->full_name) ==
        used_layouts.end()) {
      continue;
    }

    if (!is_variant_displayed) {
      auto short_name = layout->short_name;
      if (found_by_short_names.count(short_name) > 0) {
        found_by_short_names[short_name].push_back(layout);
      } else {
        found_by_short_names[short_name] = {layout};
      }
    }

    layouts_map_.emplace(layout->full_name, *layout);
  }

  if (is_variant_displayed || found_by_short_names.size() == 0) {
    return;
  }

  std::map<std::string, int> short_name_to_number_map;
  for (const auto& used_layout_name : used_layouts) {
    auto found = layouts_map_.find(used_layout_name);
    if (found == layouts_map_.end()) continue;
    auto used_layout = &found->second;
    auto layouts_with_same_name_list = found_by_short_names[used_layout->short_name];
    if (layouts_with_same_name_list.size() < 2) {
      continue;
    }

    if (short_name_to_number_map.count(used_layout->short_name) == 0) {
      short_name_to_number_map[used_layout->short_name] = 1;
    }

    if (displayed_short_flag != static_cast<std::byte>(0)) {
      int& number = short_name_to_number_map[used_layout->short_name];
      used_layout->short_name = used_layout->short_name + std::to_string(number);
      used_layout->short_description = used_layout->short_description + std::to_string(number);
      ++number;
    }
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

  auto description = std::string(rxkb_layout_get_description(xkb_layout_));
  auto name = std::string(rxkb_layout_get_name(xkb_layout_));
  auto variant_ = rxkb_layout_get_variant(xkb_layout_);
  std::string variant = variant_ == nullptr ? "" : std::string(variant_);
  auto short_description_ = rxkb_layout_get_brief(xkb_layout_);
  std::string short_description;
  if (short_description_ != nullptr) {
    short_description = std::string(short_description_);
    base_layouts_by_name_.emplace(name, xkb_layout_);
  } else {
    auto base_layout = base_layouts_by_name_[name];
    short_description =
        base_layout == nullptr ? "" : std::string(rxkb_layout_get_brief(base_layout));
  }
  delete layout_;
  layout_ = new Layout{description, name, variant, short_description};
  return layout_;
}

Language::XKBContext::~XKBContext() {
  rxkb_context_unref(context_);
  delete layout_;
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
}  // namespace waybar::modules::sway
