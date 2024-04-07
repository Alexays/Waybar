#pragma once

#include <wayland-client.h>

#include "ALabel.hpp"
#include "bar.hpp"
#include "river-status-unstable-v1-client-protocol.h"

namespace wabar::modules::river {

class Mode : public wabar::ALabel {
 public:
  Mode(const std::string &, const wabar::Bar &, const Json::Value &);
  virtual ~Mode();

  // Handlers for wayland events
  void handle_mode(const char *mode);

  struct zriver_status_manager_v1 *status_manager_;
  struct wl_seat *seat_;

 private:
  const wabar::Bar &bar_;
  std::string mode_;
  struct zriver_seat_status_v1 *seat_status_;
};

} /* namespace wabar::modules::river */
