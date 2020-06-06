#pragma once

#include <gtkmm/button.h>
#include <wayland-client.h>

#include "AModule.hpp"
#include "bar.hpp"
#include "river-status-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

namespace waybar::modules::river {

class Tags : public waybar::AModule {
 public:
  Tags(const std::string &, const waybar::Bar &, const Json::Value &);
  ~Tags();

  // Handlers for wayland events
  void handle_focused_tags(uint32_t tags);
  void handle_view_tags(struct wl_array *tags);

  struct zriver_status_manager_v1 *status_manager_;

 private:
  const waybar::Bar &      bar_;
  Gtk::Box                 box_;
  std::vector<Gtk::Button> buttons_;
  struct zriver_output_status_v1 *output_status_;
};

} /* namespace waybar::modules::river */
