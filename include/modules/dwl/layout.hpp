#pragma once

#include <fmt/format.h>

#include <string>

#include "AAppIconLabel.hpp"
#include "bar.hpp"
#include "dwl-ipc-unstable-v2-client-protocol.h"
#include "util/json.hpp"

namespace waybar::modules::dwl {

class Layout : public ALabel, public sigc::trackable {
 public:
  Layout(const std::string &, const waybar::Bar &, const Json::Value &);
  ~Layout();

  void handle_layout(const uint32_t layout);
  void handle_layout_symbol(const char *layout_symbol);
  void handle_frame();

  struct zdwl_ipc_manager_v2 *status_manager_;

 private:
  const Bar &bar_;

  std::string layout_symbol_;
  uint32_t layout_;

  struct zdwl_ipc_output_v2 *output_status_;
};

}  // namespace waybar::modules::dwl
