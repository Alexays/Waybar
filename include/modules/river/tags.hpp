#pragma once

#include <gtkmm/button.h>
#include <wayland-client.h>

#include "AModule.hpp"
#include "bar.hpp"
#include "river-control-unstable-v1-client-protocol.h"
#include "river-status-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

namespace waybar::modules::river {

class Tags : public waybar::AModule {
 public:
  Tags(const std::string &, const waybar::Bar &, const Json::Value &);
  virtual ~Tags();

  // Handlers for wayland events
  void handle_focused_tags(uint32_t tags);
  void handle_view_tags(struct wl_array *tags);
  void handle_urgent_tags(uint32_t tags);
  void handle_focused_output(struct wl_output *output);
  void handle_unfocused_output(struct wl_output *output);

  void handle_primary_clicked(uint32_t tag);
  bool handle_button_press(GdkEventButton *event_button, uint32_t tag);

  struct zriver_status_manager_v1 *status_manager_;
  struct zriver_control_v1 *control_;
  struct wl_seat *seat_;

 private:
  const waybar::Bar &bar_;
  struct wl_output *output_;          // stores the output this module belongs to
  Gtk::Box box_;
  std::vector<Gtk::Button> buttons_;
  struct zriver_output_status_v1 *output_status_;
  struct zriver_seat_status_v1 *seat_status_;
};

} /* namespace waybar::modules::river */
