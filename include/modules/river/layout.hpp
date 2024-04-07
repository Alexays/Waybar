#pragma once

#include <wayland-client.h>

#include "ALabel.hpp"
#include "bar.hpp"
#include "river-status-unstable-v1-client-protocol.h"

namespace wabar::modules::river {

class Layout : public wabar::ALabel {
 public:
  Layout(const std::string &, const wabar::Bar &, const Json::Value &);
  virtual ~Layout();

  // Handlers for wayland events
  void handle_name(const char *name);
  void handle_clear();
  void handle_focused_output(struct wl_output *output);
  void handle_unfocused_output(struct wl_output *output);

  struct zriver_status_manager_v1 *status_manager_;
  struct wl_seat *seat_;

 private:
  const wabar::Bar &bar_;
  struct wl_output *output_;          // stores the output this module belongs to
  struct wl_output *focused_output_;  // stores the currently focused output
  struct zriver_output_status_v1 *output_status_;
  struct zriver_seat_status_v1 *seat_status_;
};

} /* namespace wabar::modules::river */
