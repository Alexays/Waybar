#pragma once

#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <json/value.h>

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

class Workspaces : public AModule, public EventHandler {
 public:
  Workspaces(const std::string&, const waybar::Bar&, const Json::Value&);
  ~Workspaces() override;
  void update() override;
  void init();

  auto allOutputs() const -> bool { return m_allOutputs; }
  auto showSpecial() const -> bool { return m_showSpecial; }
  auto activeOnly() const -> bool { return m_activeOnly; }
  auto specialVisibleOnly() const -> bool { return m_specialVisibleOnly; }
  auto persistentOnly() const -> bool { return m_persistentOnly; }
  auto moveToMonitor() const -> bool { return m_moveToMonitor; }

  auto getBarOutput() const -> std::string { return m_bar.output->name; }

  std::string getRewrite(std::string window_class, std::string window_title);
  std::string& getWindowSeparator() { return m_formatWindowSeparator; }
  bool isWorkspaceIgnored(std::string const& workspace_name);

  bool windowRewriteConfigUsesTitle() const { return m_anyWindowRewriteRuleUsesTitle; }

 private:
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

  void onConfigReloaded();

  int windowRewritePriorityFunction(std::string const& window_rule);

  // event payload management
  template <typename... Args>
  static std::string makePayload(Args const&... args);
  static std::pair<std::string, std::string> splitDoublePayload(std::string const& payload);
  static std::tuple<std::string, std::string, std::string> splitTriplePayload(
      std::string const& payload);

  // Update methods
  void doUpdate();
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

  bool m_allOutputs = false;
  bool m_showSpecial = false;
  bool m_activeOnly = false;
  bool m_specialVisibleOnly = false;
  bool m_persistentOnly = false;
  bool m_moveToMonitor = false;
  Json::Value m_persistentWorkspaceConfig;

  // Map for windows stored in workspaces not present in the current bar.
  // This happens when the user has multiple monitors (hence, multiple bars)
  // and doesn't share windows across bars (a.k.a `all-outputs` = false)
  std::map<WindowAddress, std::string> m_orphanWindowMap;

  enum class SortMethod { ID, NAME, NUMBER, SPECIAL_CENTERED, DEFAULT };
  util::EnumParser<SortMethod> m_enumParser;
  SortMethod m_sortBy = SortMethod::DEFAULT;
  std::map<std::string, SortMethod> m_sortMap = {{"ID", SortMethod::ID},
                                                 {"NAME", SortMethod::NAME},
                                                 {"NUMBER", SortMethod::NUMBER},
                                                 {"SPECIAL-CENTERED", SortMethod::SPECIAL_CENTERED},
                                                 {"DEFAULT", SortMethod::DEFAULT}};

  std::string m_format;

  std::map<std::string, std::string> m_iconsMap;
  util::RegexCollection m_windowRewriteRules;
  bool m_anyWindowRewriteRuleUsesTitle = false;
  std::string m_formatWindowSeparator;

  bool m_withIcon;
  uint64_t m_monitorId;
  int m_activeWorkspaceId;
  std::string m_activeSpecialWorkspaceName;
  std::vector<std::unique_ptr<Workspace>> m_workspaces;
  std::vector<std::pair<Json::Value, Json::Value>> m_workspacesToCreate;
  std::vector<std::string> m_workspacesToRemove;
  std::vector<WindowCreationPayload> m_windowsToCreate;

  std::vector<std::regex> m_ignoreWorkspaces;

  std::mutex m_mutex;
  const Bar& m_bar;
  Gtk::Box m_box;
  IPC& m_ipc;
};

}  // namespace waybar::modules::hyprland
