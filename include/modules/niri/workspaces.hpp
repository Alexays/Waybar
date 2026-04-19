#pragma once

#include <gtkmm/box.h>
#include <json/value.h>

#include <memory>
#include <vector>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/niri/backend.hpp"
#include "modules/niri/workspace.hpp"

namespace waybar::modules::niri {

class Workspaces : public AModule, public EventHandler {
 public:
  Workspaces(const std::string& id, const Bar& bar, const Json::Value& config);
  ~Workspaces() override;

  void update() override;

  const Json::Value& config() const { return config_; }
  const Bar& bar() const { return bar_; }

  std::string getIcon(const std::string& value, const Json::Value& ws) const;

 private:
  void onEvent(const Json::Value& ev) override;
  void doUpdate();

  void createWorkspace(const Json::Value& workspace_data);

  const Bar& bar_;
  Gtk::Box box_;

  std::vector<std::unique_ptr<Workspace>> workspaces_;
};

}  // namespace waybar::modules::niri