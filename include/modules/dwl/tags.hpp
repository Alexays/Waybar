#pragma once

#include <gtkmm/button.h>

#include "AModule.hpp"
#include "bar.hpp"
#include "dwl-ipc-unstable-v2-client-protocol.h"

namespace waybar::modules::dwl {

class Tags final : public waybar::AModule {
 public:
  Tags(const std::string &, const waybar::Bar &, const Json::Value &);
  virtual ~Tags();
  Gtk::Widget &root() override;

  // Handlers for wayland events
  void handle_view_tags(uint32_t tag, uint32_t state, uint32_t clients, uint32_t focused);

  struct zdwl_ipc_manager_v2 *status_manager_;
  struct wl_seat *seat_;

 private:
  const waybar::Bar &bar_;
  Gtk::Box box_;
  std::vector<Gtk::Button> buttons_;
  struct zdwl_ipc_output_v2 *output_status_;

  void handle_primary_clicked(uint32_t tag);
  void handle_secondary_clicked(int n_press, double dx, double dy, uint32_t tag);
};

} /* namespace waybar::modules::dwl */
