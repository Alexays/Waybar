#include "modules/sway/language.hpp"

#include <fmt/core.h>
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
  if (res.type != static_cast<uint32_t>(IPC_GET_INPUTS)) {
    return;
  }

  try {
    std::lock_guard<std::mutex> lock(mutex_);
    auto                        payload = parser_.parse(res.payload);
    std::vector<std::string>    used_layouts;
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
  if (res.type != static_cast<uint32_t>(IPC_EVENT_INPUT)) {
    return;
  }

  try {
    std::lock_guard<std::mutex> lock(mutex_);
    auto                        payload = parser_.parse(res.payload)["input"];
    if (payload["type"].asString() == "keyboard") {
      set_current_layout(payload[XKB_ACTIVE_LAYOUT_NAME_KEY].asString());
    }
    dp.emit();
  } catch (const std::exception& e) {
    spdlog::error("Language: {}", e.what());
  }
}

auto Language::update() -> void {
  auto display_layout = trim(fmt::format(format_,
                                         fmt::arg("short", layout_.short_name),
                                         fmt::arg("long", layout_.full_name),
                                         fmt::arg("variant", layout_.variant)));
  label_.set_markup(display_layout);
  if (tooltipEnabled()) {
    label_.set_tooltip_markup(display_layout);
  }

  event_box_.show();

  // Call parent update
  ALabel::update();
}

auto Language::set_current_layout(std::string current_layout) -> void {
  layout_ = layouts_map_[current_layout];
}

auto Language::init_layouts_map(const std::vector<std::string>& used_layouts) -> void {
  std::map<std::string, std::vector<Layout*>> found_by_short_names;
  auto                                        layout = xkb_context_.next_layout();
  for (; layout != nullptr; layout = xkb_context_.next_layout()) {
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
    auto used_layout = &layouts_map_.find(used_layout_name)->second;
    auto layouts_with_same_name_list = found_by_short_names[used_layout->short_name];
		spdlog::info("SIZE: " + std::to_string(layouts_with_same_name_list.size()));
    if (layouts_with_same_name_list.size() < 2) {
      continue;
    }

    if (short_name_to_number_map.count(used_layout->short_name) == 0) {
      short_name_to_number_map[used_layout->short_name] = 1;
    }

    used_layout->short_name =
        used_layout->short_name + std::to_string(short_name_to_number_map[used_layout->short_name]++);
  }
}

Language::XKBContext::XKBContext() {
  context_ = rxkb_context_new(RXKB_CONTEXT_NO_DEFAULT_INCLUDES);
  rxkb_context_include_path_append_default(context_);
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

  auto        description = std::string(rxkb_layout_get_description(xkb_layout_));
  auto        name = std::string(rxkb_layout_get_name(xkb_layout_));
  auto        variant_ = rxkb_layout_get_variant(xkb_layout_);
  std::string variant = variant_ == nullptr ? "" : std::string(variant_);

  layout_ = new Layout{description, name, variant};
  return layout_;
}

Language::XKBContext::~XKBContext() { rxkb_context_unref(context_); }
}  // namespace waybar::modules::sway
