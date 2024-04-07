#pragma once

#include <gtkmm/button.h>
#include <wayland-client.h>

#include "ALabel.hpp"
#include "bar.hpp"
#include "river-status-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

namespace wabar::modules::river {

class Window : public wabar::ALabel {
 public:
  Window(const std::string &, const wabar::Bar &, const Json::Value &);
  virtual ~Window();

  // Handlers for wayland events
  void handle_focused_view(const char *title);
  void handle_focused_output(struct wl_output *output);
  void handle_unfocused_output(struct wl_output *output);

  struct zriver_status_manager_v1 *status_manager_;
  struct wl_seat *seat_;

 private:
  const wabar::Bar &bar_;
  struct wl_output *output_;          // stores the output this module belongs to
  struct wl_output *focused_output_;  // stores the currently focused output
  struct zriver_seat_status_v1 *seat_status_;
};

} /* namespace wabar::modules::river */
