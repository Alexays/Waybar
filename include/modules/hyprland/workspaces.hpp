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

static const std::string SPECIAL_QUALIFIER_PREFIX = "special:";
static const int SPECIAL_QUALIFIER_PREFIX_LEN = SPECIAL_QUALIFIER_PREFIX.length();

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
  bool isActive() const { return m_active; };
  bool isSpecial() const { return m_isSpecial; };
  bool isPersistent() const { return m_isPersistent; };
  bool isVisible() const { return m_isVisible; };
  bool isEmpty() const { return m_windows == 0; };
  bool isUrgent() const { return m_isUrgent; };

  bool handleClicked(GdkEventButton* bt) const;
  void setActive(bool value = true) { m_active = value; };
  void setPersistent(bool value = true) { m_isPersistent = value; };
  void setUrgent(bool value = true) { m_isUrgent = value; };
  void setVisible(bool value = true) { m_isVisible = value; };
  void setWindows(uint value) { m_windows = value; };
  void setName(std::string const& value) { m_name = value; };
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
  bool m_active = false;
  bool m_isSpecial = false;
  bool m_isPersistent = false;
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

  auto getBarOutput() const -> std::string { return m_bar.output->name; }

  std::string getRewrite(std::string window_class, std::string window_title);
  std::string& getWindowSeparator() { return m_formatWindowSeparator; }
  bool isWorkspaceIgnored(std::string const& workspace_name);

  bool windowRewriteConfigUsesTitle() const { return m_anyWindowRewriteRuleUsesTitle; }

 private:
  void onEvent(const std::string& e) override;
  void updateWindowCount();
  void sortWorkspaces();
  auto locateWorkspace(const std::string& workspaceName);
  void createWorkspace(Json::Value const& workspace_data,
                       Json::Value const& clients_data = Json::Value::nullRef);
  void removeWorkspace(std::string const& name);
  void setUrgentWorkspace(std::string const& windowaddress);
  void parseConfig(const Json::Value& config);
  void registerIpc();

  // workspace events
  void onWorkspaceActivated(std::string const& payload);
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

  int windowRewritePriorityFunction(std::string const& window_rule);

  void doUpdate();

  void extendOrphans(int workspaceId, Json::Value const& clientsJson);
  void registerOrphanWindow(WindowCreationPayload create_window_paylod);

  bool m_allOutputs = false;
  bool m_showSpecial = false;
  bool m_activeOnly = false;

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

  void fillPersistentWorkspaces();
  void createPersistentWorkspaces();
  std::vector<std::string> m_persistentWorkspacesToCreate;
  bool m_persistentCreated = false;

  std::string m_format;

  std::map<std::string, std::string> m_iconsMap;
  util::RegexCollection m_windowRewriteRules;
  bool m_anyWindowRewriteRuleUsesTitle = false;
  std::string m_formatWindowSeparator;

  bool m_withIcon;
  uint64_t m_monitorId;
  std::string m_activeWorkspaceName;
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
