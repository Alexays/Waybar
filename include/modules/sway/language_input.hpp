#pragma once

#include <json/json.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace waybar::modules::sway {

struct KeyboardInputsSnapshot {
  std::vector<std::string> used_layouts;
  std::string active_layout;
  bool found_keyboard{false};
  bool has_physical_keyboard{false};
};

struct InputEventAction {
  bool refresh_inputs{false};
  bool update_layout{false};
  std::string active_layout;
};

inline bool is_virtual_keyboard(const Json::Value& input) {
  if (!input.isObject() || !input["identifier"].isString()) {
    return false;
  }
  const auto identifier = input["identifier"].asString();
  if (!identifier.starts_with("0:0:")) {
    return false;
  }
  std::string lower_id = identifier;
  std::transform(lower_id.begin(), lower_id.end(), lower_id.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return lower_id.find("virtual") != std::string::npos;
}

inline KeyboardInputsSnapshot collect_keyboard_layouts(const Json::Value& inputs) {
  KeyboardInputsSnapshot snapshot;
  if (!inputs.isArray()) {
    return snapshot;
  }

  for (Json::ArrayIndex i = 0; i < inputs.size(); ++i) {
    if (!inputs[i].isObject() || !inputs[i]["type"].isString()) {
      continue;
    }
    if (inputs[i]["type"].asString() == "keyboard" && !is_virtual_keyboard(inputs[i])) {
      snapshot.has_physical_keyboard = true;
      break;
    }
  }

  Json::ArrayIndex max_id = 0;
  Json::ArrayIndex max_layouts = 0;
  for (Json::ArrayIndex i = 0; i < inputs.size(); ++i) {
    if (!inputs[i].isObject() || !inputs[i]["type"].isString() ||
        inputs[i]["type"].asString() != "keyboard") {
      continue;
    }
    // Ignore virtual keyboards when at least one physical keyboard is present (#5517),
    // but fall back to virtual keyboards on headless/tablet setups that have none.
    if (snapshot.has_physical_keyboard && is_virtual_keyboard(inputs[i])) {
      continue;
    }
    const auto& names = inputs[i]["xkb_layout_names"];
    if (!names.isArray()) {
      continue;
    }
    if (!snapshot.found_keyboard || names.size() > max_layouts) {
      max_layouts = names.size();
      max_id = i;
      snapshot.found_keyboard = true;
    }
    for (const auto& layout : names) {
      if (!layout.isString()) {
        continue;
      }
      const auto name = layout.asString();
      if (std::find(snapshot.used_layouts.begin(), snapshot.used_layouts.end(), name) ==
          snapshot.used_layouts.end()) {
        snapshot.used_layouts.push_back(name);
      }
    }
  }

  if (snapshot.found_keyboard && inputs[max_id]["xkb_active_layout_name"].isString()) {
    snapshot.active_layout = inputs[max_id]["xkb_active_layout_name"].asString();
  }
  return snapshot;
}

inline InputEventAction classify_input_event(const Json::Value& root, bool has_physical_keyboard) {
  InputEventAction action;
  if (!root.isObject() || !root["change"].isString() || !root["input"].isObject()) {
    return action;
  }
  const auto change = root["change"].asString();
  const auto& payload = root["input"];
  if (!payload["type"].isString() || payload["type"].asString() != "keyboard") {
    return action;
  }
  if (has_physical_keyboard && is_virtual_keyboard(payload)) {
    return action;
  }

  const bool removed = (change == "removed");
  action.refresh_inputs = (change == "added" || removed || change == "xkb_keymap");
  if (!removed && payload["xkb_active_layout_name"].isString()) {
    action.update_layout = true;
    action.active_layout = payload["xkb_active_layout_name"].asString();
  }
  return action;
}

}  // namespace waybar::modules::sway
