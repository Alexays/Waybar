#include "modules/hyprland/windowcreationpayload.hpp"

#include <json/value.h>
#include <spdlog/spdlog.h>

#include <string>
#include <utility>
#include <variant>

#include "modules/hyprland/workspaces.hpp"

namespace waybar::modules::hyprland {

WindowCreationPayload::WindowCreationPayload(Json::Value const &client_data)
    : m_window(std::make_pair(client_data["class"].asString(), client_data["title"].asString())),
      m_windowAddress(client_data["address"].asString()),
      m_workspaceName(client_data["workspace"]["name"].asString()) {
  clearAddr();
  clearWorkspaceName();
}

WindowCreationPayload::WindowCreationPayload(std::string workspace_name,
                                             WindowAddress window_address, std::string window_repr)
    : m_window(std::move(window_repr)),
      m_windowAddress(std::move(window_address)),
      m_workspaceName(std::move(workspace_name)) {
  clearAddr();
  clearWorkspaceName();
}

WindowCreationPayload::WindowCreationPayload(std::string workspace_name,
                                             WindowAddress window_address, std::string window_class,
                                             std::string window_title)
    : m_window(std::make_pair(std::move(window_class), std::move(window_title))),
      m_windowAddress(std::move(window_address)),
      m_workspaceName(std::move(workspace_name)) {
  clearAddr();
  clearWorkspaceName();
}

void WindowCreationPayload::clearAddr() {
  // substr(2, ...) is necessary because Hyprland's JSON follows this format:
  // 0x{ADDR}
  // While Hyprland's IPC follows this format:
  // {ADDR}
  static const std::string ADDR_PREFIX = "0x";
  static const int ADDR_PREFIX_LEN = ADDR_PREFIX.length();

  if (m_windowAddress.starts_with(ADDR_PREFIX)) {
    m_windowAddress =
        m_windowAddress.substr(ADDR_PREFIX_LEN, m_windowAddress.length() - ADDR_PREFIX_LEN);
  }
}

void WindowCreationPayload::clearWorkspaceName() {
  // The workspace name may optionally feature "special:" at the beginning.
  // If so, we need to remove it because the workspace is saved WITHOUT the
  // special qualifier. The reasoning is that not all of Hyprland's IPC events
  // use this qualifier, so it's better to be consistent about our uses.

  static const std::string SPECIAL_QUALIFIER_PREFIX = "special:";
  static const int SPECIAL_QUALIFIER_PREFIX_LEN = SPECIAL_QUALIFIER_PREFIX.length();

  if (m_workspaceName.starts_with(SPECIAL_QUALIFIER_PREFIX)) {
    m_workspaceName = m_workspaceName.substr(
        SPECIAL_QUALIFIER_PREFIX_LEN, m_workspaceName.length() - SPECIAL_QUALIFIER_PREFIX_LEN);
  }

  std::size_t spaceFound = m_workspaceName.find(' ');
  if (spaceFound != std::string::npos) {
    m_workspaceName.erase(m_workspaceName.begin() + spaceFound, m_workspaceName.end());
  }
}

bool WindowCreationPayload::isEmpty(Workspaces &workspace_manager) {
  if (std::holds_alternative<Repr>(m_window)) {
    return std::get<Repr>(m_window).empty();
  }
  if (std::holds_alternative<ClassAndTitle>(m_window)) {
    auto [window_class, window_title] = std::get<ClassAndTitle>(m_window);
    return (window_class.empty() &&
            (!workspace_manager.windowRewriteConfigUsesTitle() || window_title.empty()));
  }
  // Unreachable
  spdlog::error("WorkspaceWindow::isEmpty: Unreachable");
  throw std::runtime_error("WorkspaceWindow::isEmpty: Unreachable");
}

int WindowCreationPayload::incrementTimeSpentUncreated() { return m_timeSpentUncreated++; }

void WindowCreationPayload::moveToWorkspace(std::string &new_workspace_name) {
  m_workspaceName = new_workspace_name;
}

std::string WindowCreationPayload::repr(Workspaces &workspace_manager) {
  if (std::holds_alternative<Repr>(m_window)) {
    return std::get<Repr>(m_window);
  }
  if (std::holds_alternative<ClassAndTitle>(m_window)) {
    auto [window_class, window_title] = std::get<ClassAndTitle>(m_window);
    return workspace_manager.getRewrite(window_class, window_title);
  }
  // Unreachable
  spdlog::error("WorkspaceWindow::repr: Unreachable");
  throw std::runtime_error("WorkspaceWindow::repr: Unreachable");
}

}  // namespace waybar::modules::hyprland
