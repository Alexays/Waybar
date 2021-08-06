#pragma once

#include <fmt/format.h>
#include <xkbcommon/xkbregistry.h>

#include <map>
#include <string>
#include <unordered_map>

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
  struct Layout {
    std::string full_name;
    std::string short_name;
    std::string variant;
  };

  class XKBContext {
   public:
	   XKBContext();
	   XKBContext(const XKBContext&) = delete;
	   XKBContext(XKBContext&&) = delete;
	   ~XKBContext();
	   bool first();
	   bool next();
	   std::string full_name();
	   std::string short_name();
	   std::string variant();
   private:
	rxkb_context* context_ = nullptr;
	rxkb_layout* xkb_layout_ = nullptr;
  };

  struct Identifier {
    std::string full_name;
    unsigned event_count = 0U;
  };

  void onEvent(const struct Ipc::ipc_response&);
  void onCmd(const struct Ipc::ipc_response&);

  auto init_layouts_map() -> void;

  const static std::string XKB_LAYOUT_NAMES_KEY;
  const static std::string XKB_ACTIVE_LAYOUT_NAME_KEY;

  Layout                        layout_;
  std::string chosen_identifier_;
  std::string tooltip_format_;
  std::map<std::string, Layout> layouts_map_;
  std::unordered_map<std::string, Identifier> identifiers_map_;
  XKBContext xkb_context_;

  util::JsonParser         parser_;
  std::mutex               mutex_;
  Ipc                      ipc_;
};

}  // namespace waybar::modules::sway
