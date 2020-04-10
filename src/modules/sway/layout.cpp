#include "modules/sway/layout.hpp"
#include <spdlog/spdlog.h>

namespace waybar::modules::sway {

Layout::Layout(const std::string& id, const Json::Value& config)
    : ALabel(config, "layout", id, "{}", 0) {

  keyboard_id_ = config_["keyboard_id"].isString()
    ? config_["keyboard_id"].asString()
    : "1:1:AT_Translated_Set_2_keyboard";

  ipc_.subscribe(R"(["input"])");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &Layout::onEvent));
  ipc_.signal_cmd.connect(sigc::mem_fun(*this, &Layout::onCmd));
  ipc_.sendCmd(IPC_GET_INPUTS);
  // Launch worker
  worker();
}

void Layout::onEvent(const struct Ipc::ipc_response& res) {
  if (res.type != static_cast<uint32_t>(IPC_EVENT_INPUT)) {
    return;
  }

  auto payload = parser_.parse(res.payload);

  if ((payload["change"] != "xkb_layout" || payload["change"] != "xkb_keymap" ) &&
      payload["input"]["identifier"] == keyboard_id_) {
    layout_ = payload["input"]["xkb_active_layout_name"].asString();
    dp.emit();
  }
}

void Layout::onCmd(const struct Ipc::ipc_response &res) {
  if (res.type != IPC_GET_INPUTS) {
    return;
  }

  auto payload = parser_.parse(res.payload);

  for (auto keyboard : payload) {
    if (keyboard["identifier"] == keyboard_id_) {
      layout_ = keyboard["xkb_active_layout_name"].asString();
      dp.emit();
      break;
    }
  }
}

void Layout::worker() {
  thread_ = [this] {
    try {
      ipc_.handleEvent();
    } catch (const std::exception& e) {
      spdlog::error("Layout: {}", e.what());
    }
  };
}

auto Layout::update() -> void {
  auto short_names = getShortNames();
  auto short_description = std::get<0>(short_names);
  auto short_variant = std::get<1>(short_names);

  if (layout_.empty()) {
    event_box_.hide();
  } else {
    label_.set_markup(fmt::format(format_,
        fmt::arg("long", layout_),
        fmt::arg("short", short_description),
        fmt::arg("variant", short_variant)));
    if (tooltipEnabled()) {
      if (config_["tooltip-format"].isString()) {
        auto tooltip_format = config_["tooltip-format"].asString();
        label_.set_tooltip_text(fmt::format(tooltip_format,
            fmt::arg("long", layout_),
            fmt::arg("short", short_description),
            fmt::arg("variant", short_variant)));
      } else {
        label_.set_tooltip_text(layout_);
      }
    }
    event_box_.show();
  }
}

Layout::ShortNames Layout::getShortNames() {
  try {
    return memoizedShortNames_.at(layout_);
  } catch (std::out_of_range &e) {
    spdlog::debug("Layout: Getting short names from XKB file for {}.", layout_);
    auto shortNames = fromFileGetShortNames();
    memoizedShortNames_[layout_] = shortNames;
    return shortNames;
  }
}

Layout::ShortNames Layout::fromFileGetShortNames() {
  std::string short_description;
  std::string short_variant;

  pugi::xml_document doc;
  doc.load_file(xbk_file_.c_str());

  for (auto xkb_layout_xpath : doc.select_nodes("/xkbConfigRegistry/layoutList/layout")) {
    auto xkb_layout_config = xkb_layout_xpath.node().child("configItem");

    if (xkb_layout_config.child_value("description") == layout_) {
      return std::make_tuple(
        xkb_layout_config.child_value("shortDescription"),
        xkb_layout_config.child_value("name")
      );
    }

    for (auto xkb_variant_xpath : xkb_layout_xpath.node().select_nodes("variantList/variant")) {
      auto xkb_variant_config = xkb_variant_xpath.node().child("configItem");

      if (xkb_variant_config.child_value("description") == layout_) {
        return std::make_tuple(
          xkb_layout_config.child_value("shortDescription"),
          xkb_variant_config.child_value("name")
        );
      }
    }
  }

  spdlog::warn("Layout: Could not find layout \"{}\" in XKB file.", layout_);
  return std::make_tuple("N/A", "N/A");
}

}  // namespace waybar::modules::sway
