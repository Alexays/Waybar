#pragma once

#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <json/value.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <variant>
#include <vector>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/hyprland/backend.hpp"
#include "modules/hyprland/windowcreationpayload.hpp"
#include "util/enum.hpp"
#include "util/regex_collection.hpp"

using WindowAddress = std::string;

namespace waybar::modules::hyprland {

class Workspaces;
class Workspace {
 public:
  explicit Workspace(const Json::Value& workspace_data, Workspaces& workspace_manager,
                     const Json::Value& clients_data = Json::Value::nullRef);
  std::string& selectIcon(std::map<std::string, std::string>& icons_map);
  Gtk::Button& button() { return m_button; };

  int id() const { return m_id; };
  std::string name() const { return m_name; };
  std::string output() const { return m_output; };
  bool isActive() const { return m_isActive; };
  bool isSpecial() const { return m_isSpecial; };
  bool isPersistent() const { return m_isPersistentRule || m_isPersistentConfig; };
  bool isPersistentConfig() const { return m_isPersistentConfig; };
  bool isPersistentRule() const { return m_isPersistentRule; };
  bool isVisible() const { return m_isVisible; };
  bool isEmpty() const { return m_windows == 0; };
  bool isUrgent() const { return m_isUrgent; };

  bool handleClicked(GdkEventButton* bt) const;
  void setActive(bool value = true) { m_isActive = value; };
  void setPersistentRule(bool value = true) { m_isPersistentRule = value; };
  void setPersistentConfig(bool value = true) { m_isPersistentConfig = value; };
  void setUrgent(bool value = true) { m_isUrgent = value; };
  void setVisible(bool value = true) { m_isVisible = value; };
  void setWindows(uint value) { m_windows = value; };
  void setName(std::string const& value) { m_name = value; };
  void setOutput(std::string const& value) { m_output = value; };
  bool containsWindow(WindowAddress const& addr) const { return m_windowMap.contains(addr); }
  void insertWindow(WindowCreationPayload create_window_payload);
  std::string removeWindow(WindowAddress const& addr);
  void initializeWindowMap(const Json::Value& clients_data);

  bool onWindowOpened(WindowCreationPayload const& create_window_payload);
  std::optional<std::string> closeWindow(WindowAddress const& addr);

  void update(const std::string& format, const std::string& icon);

 private:
  Workspaces& m_workspaceManager;

  int m_id;
  std::string m_name;
  std::string m_output;
  uint m_windows;
  bool m_isActive = false;
  bool m_isSpecial = false;
  bool m_isPersistentRule = false;    // represents the persistent state in hyprland
  bool m_isPersistentConfig = false;  // represents the persistent state in the Waybar config
  bool m_isUrgent = false;
  bool m_isVisible = false;

  std::map<WindowAddress, std::string> m_windowMap;

  Gtk::Button m_button;
  Gtk::Box m_content;
  Gtk::Label m_label;
  IPC& m_ipc;
};

}  // namespace waybar::modules::hyprland
