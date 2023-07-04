#pragma once

#include <gtkmm/button.h>
#include <wayland-client.h>

#include "AModule.hpp"
#include "bar.hpp"
#include "dwl-ipc-unstable-v2-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

namespace waybar::modules::dwl {

class Tags : public waybar::AModule {
 public:
  Tags(const std::string &, const waybar::Bar &, const Json::Value &);
  virtual ~Tags();

  // Handlers for wayland events
  void handle_view_tags(uint32_t tag, uint32_t state, uint32_t clients, uint32_t focused);

  void handle_primary_clicked(uint32_t tag);
  bool handle_button_press(GdkEventButton *event_button, uint32_t tag);

  struct zdwl_ipc_manager_v2 *status_manager_;
  struct wl_seat *seat_;

 private:
  const waybar::Bar &bar_;
  Gtk::Box box_;
  std::vector<Gtk::Button> buttons_;
  struct zdwl_ipc_output_v2 *output_status_;
};

} /* namespace waybar::modules::dwl */
