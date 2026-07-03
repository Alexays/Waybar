#pragma once

#include <gtkmm/button.h>
#include <json/json.h>

#include <memory>
#include <vector>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/wayfire/backend.hpp"

namespace waybar::modules::wayfire {

class Workspaces : public AModule {
  std::shared_ptr<IPC> ipc;
  EventHandler handler;

  const Bar& bar_;
  Gtk::Box box_;
  std::vector<Gtk::Button> buttons_;

  auto handleScroll(GdkEventScroll* e) -> bool override;
  auto update() -> void override;
  auto update_box() -> void;

 public:
  Workspaces(const std::string& id, const Bar& bar, const Json::Value& config);
  ~Workspaces() override;
};

}  // namespace waybar::modules::wayfire
