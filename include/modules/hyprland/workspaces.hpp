#pragma once

#include <fmt/format.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/hyprland/backend.hpp"
#include "util/json.hpp"
#include <deque>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace waybar::modules::hyprland {

class Workspaces : public AModule, public sigc::trackable {
 public:
  Workspaces(const std::string&, const waybar::Bar&, const Json::Value&);
  ~Workspaces() = default;
  auto update() -> void;

 private:
  void onEvent(const std::string&);
  bool handleScroll(GdkEventScroll*);
  void configOnLaunch(const Json::Value&);
  void updateButtons();
  void parseInitHyprlandWorkspaces();
  Gtk::Button& addButton(const std::string&);
  std::string getIcon(const std::string&);
  std::deque<std::string> getAllSortedWS();

  bool isNumber(const std::string&);

  Gtk::Box box_;
  const Bar& bar_;
  std::deque<std::string> workspaces;
  std::deque<std::string> persistentWorkspaces;
  std::unordered_map<std::string, Gtk::Button> buttons_;
  std::string focusedWorkspace;

  std::mutex mutex_;
};

}  // namespace waybar::modules::sway
