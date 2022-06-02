#pragma once

#include <wayland-client.h>

#include "ALabel.hpp"
#include "bar.hpp"
#include "river-status-unstable-v1-client-protocol.h"

namespace waybar::modules::river {

class Mode : public waybar::ALabel {
 public:
  Mode(const std::string &, const waybar::Bar &, const Json::Value &);
  ~Mode();

  // Handlers for wayland events
  void handle_mode(const char *mode);

  struct zriver_status_manager_v1 *status_manager_;
  struct wl_seat *seat_;

 private:
  const waybar::Bar &bar_;
  std::string mode_;
  struct zriver_seat_status_v1 *seat_status_;
};

} /* namespace waybar::modules::river */
