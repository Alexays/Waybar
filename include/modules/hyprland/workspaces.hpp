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
#include "util/enum.hpp"
#include "util/regex_collection.hpp"

using WindowAddress = std::string;

namespace waybar::modules::hyprland {

class Workspaces;

class WindowCreationPayload {
 public:
  WindowCreationPayload(std::string workspace_name, WindowAddress window_address,
                        std::string window_repr);
  WindowCreationPayload(std::string workspace_name, WindowAddress window_address,
                        std::string window_class, std::string window_title);
  WindowCreationPayload(Json::Value const& client_data);

  int incrementTimeSpentUncreated();
  bool isEmpty(Workspaces& workspace_manager);
  bool reprIsReady() const { return std::holds_alternative<Repr>(m_window); }
  std::string repr(Workspaces& workspace_manager);

  std::string getWorkspaceName() const { return m_workspaceName; }
  WindowAddress getAddress() const { return m_windowAddress; }

  void moveToWorksace(std::string& new_workspace_name);

 private:
  void clearAddr();
  void clearWorkspaceName();

  using Repr = std::string;
  using ClassAndTitle = std::pair<std::string, std::string>;
  std::variant<Repr, ClassAndTitle> m_window;

  WindowAddress m_windowAddress;
  std::string m_workspaceName;

  int m_timeSpentUncreated = 0;
};

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
  void insertWindow(WindowCreationPayload create_window_paylod);
  std::string removeWindow(WindowAddress const& addr);
  void initializeWindowMap(const Json::Value& clients_data);

  bool onWindowOpened(WindowCreationPayload const& create_window_paylod);
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
};

class Workspaces : public AModule, public EventHandler {
 public:
  Workspaces(const std::string&, const waybar::Bar&, const Json::Value&);
  ~Workspaces() override;
  void update() override;
  void init();

  auto allOutputs() const -> bool { return m_allOutputs; }
  auto showSpecial() const -> bool { return m_showSpecial; }
  auto activeOnly() const -> bool { return m_activeOnly; }
  auto moveToMonitor() const -> bool { return m_moveToMonitor; }

  auto getBarOutput() const -> std::string { return m_bar.output->name; }

  std::string getRewrite(std::string window_class, std::string window_title);
  std::string& getWindowSeparator() { return m_formatWindowSeparator; }
  bool isWorkspaceIgnored(std::string const& workspace_name);

  bool windowRewriteConfigUsesTitle() const { return m_anyWindowRewriteRuleUsesTitle; }

 private:
  void onEvent(const std::string& e) override;
  void updateWindowCount();
  void sortWorkspaces();
  void createWorkspace(Json::Value const& workspaceData,
                       Json::Value const& clientsData = Json::Value::nullRef);
  void removeWorkspace(std::string const& name);
  void setUrgentWorkspace(std::string const& windowaddress);

  // Config
  void parseConfig(const Json::Value& config);
  auto populateIconsMap(const Json::Value& formatIcons) -> void;
  auto populateBoolConfig(const Json::Value& config, const std::string& key, bool& member) -> void;
  auto populateSortByConfig(const Json::Value& config) -> void;
  auto populateIgnoreWorkspacesConfig(const Json::Value& config) -> void;
  auto populatePersistentWorkspacesConfig(const Json::Value& config) -> void;
  auto populateFormatWindowSeparatorConfig(const Json::Value& config) -> void;
  auto populateWindowRewriteConfig(const Json::Value& config) -> void;

  void registerIpc();

  // workspace events
  void onWorkspaceActivated(std::string const& payload);
  void onSpecialWorkspaceActivated(std::string const& payload);
  void onWorkspaceDestroyed(std::string const& payload);
  void onWorkspaceCreated(std::string const& workspaceName,
                          Json::Value const& clientsData = Json::Value::nullRef);
  void onWorkspaceMoved(std::string const& payload);
  void onWorkspaceRenamed(std::string const& payload);

  // monitor events
  void onMonitorFocused(std::string const& payload);

  // window events
  void onWindowOpened(std::string const& payload);
  void onWindowClosed(std::string const& addr);
  void onWindowMoved(std::string const& payload);

  void onWindowTitleEvent(std::string const& payload);

  void onConfigReloaded();

  int windowRewritePriorityFunction(std::string const& window_rule);

  void doUpdate();

  void extendOrphans(int workspaceId, Json::Value const& clientsJson);
  void registerOrphanWindow(WindowCreationPayload create_window_payload);

  void initializeWorkspaces();
  void setCurrentMonitorId();
  void loadPersistentWorkspacesFromConfig(Json::Value const& clientsJson);
  void loadPersistentWorkspacesFromWorkspaceRules(const Json::Value& clientsJson);

  bool m_allOutputs = false;
  bool m_showSpecial = false;
  bool m_activeOnly = false;
  bool m_moveToMonitor = false;
  Json::Value m_persistentWorkspaceConfig;

  // Map for windows stored in workspaces not present in the current bar.
  // This happens when the user has multiple monitors (hence, multiple bars)
  // and doesn't share windows accross bars (a.k.a `all-outputs` = false)
  std::map<WindowAddress, std::string> m_orphanWindowMap;

  enum class SortMethod { ID, NAME, NUMBER, DEFAULT };
  util::EnumParser<SortMethod> m_enumParser;
  SortMethod m_sortBy = SortMethod::DEFAULT;
  std::map<std::string, SortMethod> m_sortMap = {{"ID", SortMethod::ID},
                                                 {"NAME", SortMethod::NAME},
                                                 {"NUMBER", SortMethod::NUMBER},
                                                 {"DEFAULT", SortMethod::DEFAULT}};

  std::string m_format;

  std::map<std::string, std::string> m_iconsMap;
  util::RegexCollection m_windowRewriteRules;
  bool m_anyWindowRewriteRuleUsesTitle = false;
  std::string m_formatWindowSeparator;

  bool m_withIcon;
  uint64_t m_monitorId;
  std::string m_activeWorkspaceName;
  std::string m_activeSpecialWorkspaceName;
  std::vector<std::unique_ptr<Workspace>> m_workspaces;
  std::vector<std::pair<Json::Value, Json::Value>> m_workspacesToCreate;
  std::vector<std::string> m_workspacesToRemove;
  std::vector<WindowCreationPayload> m_windowsToCreate;

  std::vector<std::regex> m_ignoreWorkspaces;

  std::mutex m_mutex;
  const Bar& m_bar;
  Gtk::Box m_box;
};

}  // namespace waybar::modules::hyprland
