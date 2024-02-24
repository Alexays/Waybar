#pragma once

#include <fmt/format.h>
#include <xkbcommon/xkbregistry.h>

#include <map>
#include <string>
#include <type_traits>
#include <vector>

#include "ALabel.hpp"
#include "bar.hpp"
#include "client.hpp"
#include "modules/sway/ipc/client.hpp"
#include "util/json.hpp"

namespace waybar::modules::sway {

class Language : public ALabel, public sigc::trackable {
 public:
  Language(const std::string& id, const Json::Value& config);
  virtual ~Language() = default;
  auto update() -> void override;

 private:
  enum VisibleFields {
    None = 0,
    ShortName = 1,
    ShortDescription = 1 << 1,
    Variant = 1 << 2,
  };

  struct Layout {
    std::string full_name;
    std::string short_name;
    std::string variant;
    std::string short_description;
    std::string country_flag() const;

    Layout() = default;
    Layout(rxkb_layout*);

    void addShortNameSuffix(std::string_view suffix);
  };

  class XKBContext {
   public:
    XKBContext();
    ~XKBContext();
    auto next_layout() -> Layout*;

   private:
    rxkb_context* context_ = nullptr;
    rxkb_layout* xkb_layout_ = nullptr;
    std::unique_ptr<Layout> layout_;
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
  bool hide_single_;
  std::underlying_type_t<VisibleFields> visible_fields = VisibleFields::None;

  util::JsonParser parser_;
  std::mutex mutex_;
  Ipc ipc_;
};

}  // namespace waybar::modules::sway
