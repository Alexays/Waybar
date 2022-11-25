#pragma once

#include <fmt/format.h>
#include <xkbcommon/xkbregistry.h>

#include <map>
#include <string>

#include "ALabel.hpp"
#include "bar.hpp"
#include "client.hpp"
#include "modules/sway/ipc/client.hpp"
#include "util/json.hpp"

namespace waybar::modules::sway {

class Language : public ALabel, public sigc::trackable {
 public:
  Language(const std::string& id, const Json::Value& config);
  ~Language() = default;
  auto update() -> void;

 private:
  enum class DispayedShortFlag { None = 0, ShortName = 1, ShortDescription = 1 << 1 };

  struct Layout {
    std::string full_name;
    std::string short_name;
    std::string variant;
    std::string short_description;
    std::string country_flag() const;
  };

  class XKBContext {
   public:
    XKBContext();
    ~XKBContext();
    auto next_layout() -> Layout*;

   private:
    rxkb_context* context_ = nullptr;
    rxkb_layout* xkb_layout_ = nullptr;
    Layout* layout_ = nullptr;
    std::map<std::string, rxkb_layout*> base_layouts_by_name_;
  };

  void onEvent(const struct Ipc::ipc_response&);
  void onCmd(const struct Ipc::ipc_response&);

  auto set_current_layout(std::string current_layout) -> void;
  auto init_layouts_map(const std::vector<std::string>& used_layouts) -> void;

  const static std::string XKB_LAYOUT_NAMES_KEY;
  const static std::string XKB_ACTIVE_LAYOUT_NAME_KEY;

  Layout layout_;
  std::string tooltip_format_ = "";
  std::map<std::string, Layout> layouts_map_;
  bool is_variant_displayed;
  std::byte displayed_short_flag = static_cast<std::byte>(DispayedShortFlag::None);

  util::JsonParser parser_;
  std::mutex mutex_;
  Ipc ipc_;
};

}  // namespace waybar::modules::sway
