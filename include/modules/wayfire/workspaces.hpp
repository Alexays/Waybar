#pragma once

#include <gtkmm/button.h>
#include <json/json.h>

#include <memory>
#include <vector>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/wayfire/backend.hpp"

namespace waybar::modules::wayfire {

class Workspaces final : public AModule {
 public:
  Workspaces(const std::string& id, const Bar& bar, const Json::Value& config);
  ~Workspaces() override;

 private:
  auto handleScroll(double dx, double dy) -> bool override;
  auto doUpdate() -> void override;
  auto update_box() -> void;

  std::shared_ptr<IPC> ipc_;
  EventHandler handler_;

  const Bar& bar_;
  Gtk::Box box_;
  std::vector<Gtk::Button> buttons_;
};

}  // namespace waybar::modules::wayfire
