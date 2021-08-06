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
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (config.isMember("tooltip-format")) {
      tooltip_format_ = config["tooltip-format"].asString();
    }
    ipc_.subscribe(R"(["input"])");
    ipc_.signal_event.connect(sigc::mem_fun(*this, &Language::onEvent));
    ipc_.signal_cmd.connect(sigc::mem_fun(*this, &Language::onCmd));
  }
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
    const auto                  payload = parser_.parse(res.payload);
    init_layouts_map();
    bool found = false;
    Json::ArrayIndex max_index = 0, max_size = 0;
    for (Json::ArrayIndex i = 0; i < payload.size(); i++) {
      const auto element = payload[i];
      if (element["type"].asString() != "keyboard") {
        continue;
      }
      const auto full_name = element[XKB_ACTIVE_LAYOUT_NAME_KEY].asString();
      const auto identifier_name = element["identifier"].asString();
      auto& identifier_record = identifiers_map_[identifier_name];
      if (identifier_record.full_name != full_name) {
        identifier_record.full_name = full_name;
        ++identifier_record.event_count;
      }
      const auto size = element[XKB_LAYOUT_NAMES_KEY].size();
      if (size > max_size) {
        max_size = size;
        max_index = i;
        found = true;
      }
    }

    if (found) {
      const auto element = payload[max_index];
      chosen_identifier_ = element["identifier"].asString();
      spdlog::info("sway/language: initial chosen identifier \"{}\"", chosen_identifier_);

      const auto full_name = element[XKB_ACTIVE_LAYOUT_NAME_KEY].asString();
      const auto layout_iter = layouts_map_.find(full_name);
      if (layout_iter != layouts_map_.end()) {
        layout_ = layout_iter->second;
      } else {
        spdlog::error("sway/language: no layout map entry \"{}\"", full_name);
      }
    } else {
      spdlog::error("sway/language: no keyboard devices with layouts found");
    }

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
    const auto                  payload = parser_.parse(res.payload);
    const auto                  element = payload["input"];
    if (element["type"].asString() == "keyboard") {
      const auto identifier_name = element["identifier"].asString();
      const auto full_name = element[XKB_ACTIVE_LAYOUT_NAME_KEY].asString();
      // Update the record counting the full_name transitions,
      // and only trigger off events for the chosen identifier with the most transitions.
      auto& identifier_record = identifiers_map_[identifier_name];
      if (identifier_record.full_name != full_name) {
        identifier_record.full_name = full_name;
        ++identifier_record.event_count;
      }
      auto& chosen_identifier_record = identifiers_map_[chosen_identifier_];
      if (identifier_record.event_count > chosen_identifier_record.event_count) {
        chosen_identifier_ = identifier_name;
      }
      if (identifier_name == chosen_identifier_) {
        const auto layout_iter = layouts_map_.find(full_name);
        if (layout_iter != layouts_map_.end()) {
          layout_ = layout_iter->second;
        } else {
          spdlog::error("sway/language: no layout map entry \"{}\"", full_name);
        }
      }
    }
    dp.emit();
  } catch (const std::exception& e) {
    spdlog::error("Language: {}", e.what());
  }
}

auto Language::update() -> void {
  std::lock_guard<std::mutex> lock(mutex_);
  auto display_layout = trim(fmt::format(format_,
                                         fmt::arg("short", layout_.short_name),
                                         fmt::arg("long", layout_.full_name),
                                         fmt::arg("variant", layout_.variant)));
  label_.set_markup(display_layout);
  if (tooltipEnabled()) {
		if (tooltip_format_ != "") {
			auto tooltip_display_layout = trim(fmt::format(tooltip_format_,
																						 fmt::arg("short", layout_.short_name),
																						 fmt::arg("long", layout_.full_name),
																						 fmt::arg("variant", layout_.variant)));
			label_.set_tooltip_markup(tooltip_display_layout);

		} else {
			label_.set_tooltip_markup(display_layout);
		}
  }

  event_box_.show();

  // Call parent update
  ALabel::update();
}

auto Language::init_layouts_map() -> void {
  layouts_map_.clear();
  bool has_layout = xkb_context_.first();
  while (has_layout) {
    const auto full_name = xkb_context_.full_name();
    layouts_map_.emplace(full_name, Layout{full_name, xkb_context_.short_name(), xkb_context_.variant()});
    has_layout = xkb_context_.next();
  }
}

Language::XKBContext::XKBContext() {
  context_ = rxkb_context_new(RXKB_CONTEXT_NO_DEFAULT_INCLUDES);
  rxkb_context_include_path_append_default(context_);
  if (!rxkb_context_parse_default_ruleset(context_)) {
    rxkb_context_unref(context_);
    context_ = nullptr;
    spdlog::error("sway/language: parsing xkb default ruleset failed, no layout info available");
  }
}

bool Language::XKBContext::first() {
  if (context_ == nullptr) {
    return false;
  }
  xkb_layout_ = rxkb_layout_first(context_);
  return xkb_layout_ != nullptr;
}

bool Language::XKBContext::next() {
  if (xkb_layout_ == nullptr) {
    return false;
  }
  xkb_layout_ = rxkb_layout_next(xkb_layout_);
  return xkb_layout_ != nullptr;
}

static inline std::string or_empty_str(const char* s) {
  return s ? std::string(s) : std::string();
}

std::string Language::XKBContext::full_name() {
  return or_empty_str(xkb_layout_ ? rxkb_layout_get_description(xkb_layout_) : nullptr);
}

std::string Language::XKBContext::short_name() {
  return or_empty_str(xkb_layout_ ? rxkb_layout_get_name(xkb_layout_) : nullptr);
}

std::string Language::XKBContext::variant() {
  return or_empty_str(xkb_layout_ ? rxkb_layout_get_variant(xkb_layout_) : nullptr);
}

Language::XKBContext::~XKBContext() {
  if (context_ != nullptr) {
    rxkb_context_unref(context_);
  }
}
}  // namespace waybar::modules::sway
