#pragma once

#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <json/value.h>

#include <algorithm>
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
  ~Workspace();
  std::string& selectString(std::map<std::string, std::string>& string_map);
  Gtk::Button& button() { return m_button_; };

  int id() const { return m_id_; };
  std::string name() const { return m_name_; };
  std::string output() const { return m_output_; };
  bool isActive() const { return m_isActive_; };
  bool isSpecial() const { return m_isSpecial_; };
  bool isPersistent() const { return m_isPersistentRule_ || m_isPersistentConfig_; };
  bool isPersistentConfig() const { return m_isPersistentConfig_; };
  bool isPersistentRule() const { return m_isPersistentRule_; };
  bool isVisible() const { return m_isVisible_; };
  bool isUrgent() const { return m_isUrgent_; };

  void handleEnter(double x, double y);
  void handleLeave();

  void setActive(bool value = true) { m_isActive_ = value; };
  void setPersistentRule(bool value = true) { m_isPersistentRule_ = value; };
  void setPersistentConfig(bool value = true) { m_isPersistentConfig_ = value; };
  void setUrgent(bool value = true) { m_isUrgent_ = value; };
  void setVisible(bool value = true) { m_isVisible_ = value; };
  void setWindows(uint value) { m_windows_ = value; };
  void setName(std::string const& value) { m_name_ = value; };
  void setOutput(std::string const& value) { m_output_ = value; };
  bool containsWindow(WindowAddress const& addr) const {
    return std::ranges::any_of(m_windowMap_,
                               [&addr](const auto& window) { return window.address == addr; });
  };
  void insertWindow(WindowCreationPayload create_window_payload);
  void initializeWindowMap(const Json::Value& clients_data);
  void setActiveWindow(WindowAddress const& addr);

  bool onWindowOpened(WindowCreationPayload const& create_window_payload);
  std::optional<WindowRepr> closeWindow(WindowAddress const& addr);

  void doUpdate(const std::string& workspace_icon, const std::string& workspace_tooltip);

 private:
  bool isEmpty() const;
  void updateTaskbar(const std::string& workspace_icon);
  void handlePress(int n_press, double x, double y);
  void handlePress(int n_press, double x, double y, guint button, WindowAddress const& addr);
  bool shouldSkipWindow(const WindowRepr& window_repr) const;

  Workspaces& m_workspaceManager_;

  int m_id_;
  std::string m_name_;
  std::string m_prevNameClass_;
  std::string m_output_;
  uint m_windows_;
  bool m_isActive_{false};
  bool m_isSpecial_{false};
  bool m_isPersistentRule_{false};    // represents the persistent state in hyprland
  bool m_isPersistentConfig_{false};  // represents the persistent state in the Waybar config
  bool m_isUrgent_{false};
  bool m_isVisible_{false};

  sigc::connection m_hoverCheckConnection_;

  std::vector<WindowRepr> m_windowMap_;

  Gtk::Button m_button_;
  Gtk::Box m_content_;
  Gtk::Label m_labelBefore_;
  Gtk::Label m_labelAfter_;

  IPC& m_ipc_;
};

}  // namespace waybar::modules::hyprland
