#pragma once

#include <gtkmm/button.h>
#include <json/value.h>
#include <regex>
#include <vector>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/niri/backend.hpp"

namespace waybar::modules::niri {

class Workspaces : public AModule, public EventHandler {
 public:
  Workspaces(const std::string &, const Bar &, const Json::Value &);
  ~Workspaces() override;
  void update() override;

 private:
  void onEvent(const Json::Value &ev) override;
  void doUpdate();
  Gtk::Button &addButton(const Json::Value &ws);
  std::string getIcon(const std::string &value, const Json::Value &ws);
  bool isWorkspaceIgnored(const std::string &name);

  const Bar &bar_;
  Gtk::Box box_;
  // Map from niri workspace id to button.
  std::unordered_map<uint64_t, Gtk::Button> buttons_;
  // Vec of regex rules to ignore workspaces.
  std::vector<std::regex> ignoreWorkspaces_;
};

}  // namespace waybar::modules::niri
