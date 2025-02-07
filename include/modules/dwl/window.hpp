#pragma once

#include <fmt/format.h>

#include <string>

#include "AAppIconLabel.hpp"
#include "bar.hpp"
#include "dwl-ipc-unstable-v2-client-protocol.h"
#include "util/json.hpp"

namespace waybar::modules::dwl {

class Window : public AAppIconLabel, public sigc::trackable {
 public:
  Window(const std::string &, const waybar::Bar &, const Json::Value &);
  ~Window();

  void handle_title(const char *title);
  void handle_appid(const char *ppid);
  void handle_frame();

  struct zdwl_ipc_manager_v2 *status_manager_;

 private:
  const Bar &bar_;

  std::string title_;
  std::string appid_;

  struct zdwl_ipc_output_v2 *output_status_;
};

}  // namespace waybar::modules::dwl
