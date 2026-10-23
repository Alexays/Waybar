#pragma once

#include <gtkmm/button.h>
#include <gtkmm/enums.h>
#include <gtkmm/label.h>
#include <json/value.h>
#include <sigc++/connection.h>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <vector>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/hyprland/backend.hpp"
#include "modules/hyprland/windowcreationpayload.hpp"
#include "modules/hyprland/workspace.hpp"
#include "util/enum.hpp"
#include "util/regex_collection.hpp"

using WindowAddress = std::string;

namespace waybar::modules::hyprland {

class Workspaces;

class Workspaces final : public AModule, public EventHandler {
 public:
  Workspaces(const std::string&, const waybar::Bar&, const Json::Value&);
  ~Workspaces() override;

  auto allOutputs() const -> bool { return m_allOutputs_; }
  auto showSpecial() const -> bool { return m_showSpecial_; }
  auto activeOnly() const -> bool { return m_activeOnly_; }
  auto hideActive() const -> bool { return m_hideActive_; }
  auto specialVisibleOnly() const -> bool { return m_specialVisibleOnly_; }
  auto persistentOnly() const -> bool { return m_persistentOnly_; }
  auto moveToMonitor() const -> bool { return m_moveToMonitor_; }
  auto uniqueIcons() const -> bool { return m_uniqueIcons_; }
  auto enableTaskbar() const -> bool { return m_enableTaskbar_; }
  auto taskbarWithIcon() const -> bool { return m_taskbarWithIcon_; }
  auto barScroll() const -> bool { return m_barScroll_; }

  auto getBarOutput() const -> std::string { return m_bar_.output->name; }
  auto formatBefore() const -> std::string { return m_formatBefore_; }
  auto formatAfter() const -> std::string { return m_formatAfter_; }
  auto taskbarFormatBefore() const -> std::string { return m_taskbarFormatBefore_; }
  auto taskbarFormatAfter() const -> std::string { return m_taskbarFormatAfter_; }
  auto taskbarIconSize() const -> int { return m_taskbarIconSize_; }
  auto taskbarMaxIcons() const -> int { return m_taskbarMaxIcons_; }
  auto taskbarOrientation() const -> Gtk::Orientation { return m_taskbarOrientation_; }
  auto taskbarReverseDirection() const -> bool { return m_taskbarReverseDirection_; }
  auto onClickWindow() const -> std::string { return m_onClickWindow_; }
  auto getIgnoredWindows() const -> std::vector<std::regex> { return m_ignoreWindows_; }
  auto maxWindows() const -> int { return m_maxWindows_; }

  enum class ActiveWindowPosition { NONE, FIRST, LAST };
  auto activeWindowPosition() const -> ActiveWindowPosition { return m_activeWindowPosition_; }

  std::string getRewrite(const std::string& window_class, const std::string& window_title);
  std::string& getWindowSeparator() { return m_formatWindowSeparator_; }
  auto windowRewriteGroupThreshold() const -> int { return m_windowRewriteGroupThreshold_; }
  auto const& getWindowRewriteGroupFormat() const { return m_windowRewriteGroupFormat_; }
  bool isWorkspaceIgnored(std::string const& workspace_name);

  bool windowRewriteConfigUsesTitle() const { return m_anyWindowRewriteRuleUsesTitle_; }

 private:
  void doUpdate() override;
  void init();
  void onEvent(const std::string& e) override;
  void updateWindowCount();
  void sortSpecialCentered();
  void sortWorkspaces();
  void createWorkspace(Json::Value const& workspace_data,
                       Json::Value const& clients_data = Json::Value::nullRef);

  static Json::Value createMonitorWorkspaceData(std::string const& name,
                                                std::string const& monitor);
  void removeWorkspace(std::string const& workspaceString);
  void setUrgentWorkspace(std::string const& windowaddress);

  // Config
  void parseConfig(const Json::Value& config);
  auto populateIconsMap(const Json::Value& formatIcons) -> void;
  static auto populateBoolConfig(const Json::Value& config, const std::string& key, bool& member)
      -> void;
  auto populateSortByConfig(const Json::Value& config) -> void;
  auto populateIgnoreWorkspacesConfig(const Json::Value& config) -> void;
  auto populateFormatWindowSeparatorConfig(const Json::Value& config) -> void;
  auto populateWindowRewriteConfig(const Json::Value& config) -> void;
  auto populateMaxWindowsConfig(const Json::Value& config) -> void;
  auto populateWorkspaceTaskbarConfig(const Json::Value& config) -> void;

  void registerIpc();

  // workspace events
  void onWorkspaceActivated(std::string const& payload);
  void onSpecialWorkspaceActivated(std::string const& payload);
  void onWorkspaceDestroyed(std::string const& payload);
  void onWorkspaceCreated(std::string const& payload,
                          Json::Value const& clientsData = Json::Value::nullRef);
  void onWorkspaceMoved(std::string const& payload);
  void onWorkspaceRenamed(std::string const& payload);
  static std::optional<int> parseWorkspaceId(std::string const& workspaceIdStr);

  // monitor events
  void onMonitorFocused(std::string const& payload);

  // window events
  void onWindowOpened(std::string const& payload);
  void onWindowClosed(std::string const& addr);
  void onWindowMoved(std::string const& payload);

  void onWindowTitleEvent(std::string const& payload);
  void onActiveWindowChanged(WindowAddress const& payload);

  void onConfigReloaded();

  int windowRewritePriorityFunction(std::string const& window_rule);

  // event payload management
  template <typename... Args>
  static std::string makePayload(Args const&... args);
  static std::pair<std::string, std::string> splitDoublePayload(std::string const& payload);
  static std::tuple<std::string, std::string, std::string> splitTriplePayload(
      std::string const& payload);
  // scroll events
  bool handleScroll(double dx, double dy) override;

  // Update methods
  void removeWorkspacesToRemove();
  void createWorkspacesToCreate();
  static std::vector<int> getVisibleWorkspaces();
  void updateWorkspaceStates();
  bool updateWindowsToCreate();

  void extendOrphans(int workspaceId, Json::Value const& clientsJson);
  void registerOrphanWindow(WindowCreationPayload create_window_payload);

  void initializeWorkspaces();
  void setCurrentMonitorId();
  void loadPersistentWorkspacesFromConfig(Json::Value const& clientsJson);
  void loadPersistentWorkspacesFromWorkspaceRules(const Json::Value& clientsJson);

  bool m_allOutputs_{false};
  bool m_showSpecial_{false};
  bool m_activeOnly_{false};
  bool m_hideActive_{false};
  bool m_specialVisibleOnly_{false};
  bool m_persistentOnly_{false};
  bool m_moveToMonitor_{false};
  bool m_uniqueIcons_{false};
  bool m_barScroll_{false};
  Json::Value m_persistentWorkspaceConfig_;

  // Map for windows stored in workspaces not present in the current bar.
  // This happens when the user has multiple monitors (hence, multiple bars)
  // and doesn't share windows across bars (a.k.a `all-outputs` = false)
  std::map<WindowAddress, WindowRepr, std::less<>> m_orphanWindowMap_;

  enum class SortMethod { ID, NAME, NUMBER, SPECIAL_CENTERED, DEFAULT };
  util::EnumParser<SortMethod> m_enumParser_;
  SortMethod m_sortBy_{SortMethod::DEFAULT};
  std::map<std::string, SortMethod> m_sortMap_{{{"ID", SortMethod::ID},
                                                {"NAME", SortMethod::NAME},
                                                {"NUMBER", SortMethod::NUMBER},
                                                {"SPECIAL-CENTERED", SortMethod::SPECIAL_CENTERED},
                                                {"DEFAULT", SortMethod::DEFAULT}}};

  std::string m_formatBefore_;
  std::string m_formatAfter_;

  std::map<std::string, std::string> m_iconsMap_;
  std::map<std::string, std::string> m_tooltipMap_;
  bool m_withTooltip_{false};
  util::RegexCollection m_windowRewriteRules_;
  bool m_anyWindowRewriteRuleUsesTitle_{false};
  std::string m_formatWindowSeparator_;
  int m_windowRewriteGroupThreshold_{0};
  std::string m_windowRewriteGroupFormat_{"{icon}×{count}"};

  bool m_withIcon_;
  uint64_t m_monitorId_;
  int m_activeWorkspaceId_;
  std::string m_activeSpecialWorkspaceName_;
  std::vector<std::unique_ptr<Workspace>> m_workspaces_;
  std::vector<std::pair<Json::Value, Json::Value>> m_workspacesToCreate_;
  std::vector<std::string> m_workspacesToRemove_;
  std::vector<WindowCreationPayload> m_windowsToCreate_;

  bool m_enableTaskbar_{false};
  bool m_updateActiveWindow_{false};
  bool m_taskbarWithIcon_{false};
  bool m_taskbarWithTitle_{false};
  std::string m_taskbarFormatBefore_;
  std::string m_taskbarFormatAfter_;
  int m_taskbarIconSize_{16};
  int m_taskbarMaxIcons_{0};  // 0 means unlimited
  Gtk::Orientation m_taskbarOrientation_{Gtk::Orientation::HORIZONTAL};
  bool m_taskbarReverseDirection_{false};
  util::EnumParser<ActiveWindowPosition> m_activeWindowEnumParser_;
  ActiveWindowPosition m_activeWindowPosition_{ActiveWindowPosition::NONE};
  std::map<std::string, ActiveWindowPosition> m_activeWindowPositionMap_{{
      {"NONE", ActiveWindowPosition::NONE},
      {"FIRST", ActiveWindowPosition::FIRST},
      {"LAST", ActiveWindowPosition::LAST},
  }};
  std::string m_onClickWindow_;
  std::string m_currentActiveWindowAddress_;
  int m_maxWindows_{0};

  std::vector<std::regex> m_ignoreWorkspaces_;
  std::vector<std::regex> m_ignoreWindows_;

  std::mutex m_mutex_;
  const Bar& m_bar_;
  Gtk::Box m_box_;
  sigc::connection m_scrollEventConnection_;
  IPC& m_ipc_;

  // Coalesces bursts of Hyprland events into a single UI refresh. Armed and
  // disconnected only on the GTK main thread (see Workspaces::update).
  sigc::connection m_debounceTimer_;
};

}  // namespace waybar::modules::hyprland
