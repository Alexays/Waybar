#include "modules/hyprland/workspaces.hpp"

#include <json/value.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "util/regex_collection.hpp"

namespace waybar::modules::hyprland {

int Workspaces::windowRewritePriorityFunction(std::string const &window_rule) {
  // Rules that match against title are prioritized
  // Rules that don't specify if they're matching against either title or class are deprioritized
  bool const hasTitle = window_rule.find("title") != std::string::npos;
  bool const hasClass = window_rule.find("class") != std::string::npos;

  if (hasTitle && hasClass) {
    m_anyWindowRewriteRuleUsesTitle = true;
    return 3;
  }
  if (hasTitle) {
    m_anyWindowRewriteRuleUsesTitle = true;
    return 2;
  }
  if (hasClass) {
    return 1;
  }
  return 0;
}

Workspaces::Workspaces(const std::string &id, const Bar &bar, const Json::Value &config)
    : AModule(config, "workspaces", id, false, false), m_bar(bar), m_box(bar.orientation, 0) {
  modulesReady = true;
  parseConfig(config);

  m_box.set_name("workspaces");
  if (!id.empty()) {
    m_box.get_style_context()->add_class(id);
  }
  m_box.get_style_context()->add_class(MODULE_CLASS);
  event_box_.add(m_box);

  if (!gIPC) {
    gIPC = std::make_unique<IPC>();
  }

  setCurrentMonitorId();
  init();
  registerIpc();
}

auto Workspaces::parseConfig(const Json::Value &config) -> void {
  const Json::Value &configFormat = config["format"];

  m_format = configFormat.isString() ? configFormat.asString() : "{name}";
  m_withIcon = m_format.find("{icon}") != std::string::npos;

  m_withTooltip = tooltipEnabled();

  if (m_withTooltip && m_tooltipMap.empty()) {
    Json::Value tooltipFormats = config["tooltips"];
    for (std::string &name : tooltipFormats.getMemberNames()) {
      m_tooltipMap.emplace(name, tooltipFormats[name].asString());
    }
    m_tooltipMap.emplace("", "");
  }

  if (m_withIcon && m_iconsMap.empty()) {
    Json::Value formatIcons = config["format-icons"];
    for (std::string &name : formatIcons.getMemberNames()) {
      m_iconsMap.emplace(name, formatIcons[name].asString());
    }
    m_iconsMap.emplace("", "");
  }

  auto configAllOutputs = config_["all-outputs"];
  if (configAllOutputs.isBool()) {
    m_allOutputs = configAllOutputs.asBool();
  }

  auto configShowSpecial = config_["show-special"];
  if (configShowSpecial.isBool()) {
    m_showSpecial = configShowSpecial.asBool();
  }

  auto configActiveOnly = config_["active-only"];
  if (configActiveOnly.isBool()) {
    m_activeOnly = configActiveOnly.asBool();
  }

  auto configMoveToMonitor = config_["move-to-monitor"];
  if (configMoveToMonitor.isBool()) {
    m_moveToMonitor = configMoveToMonitor.asBool();
  }

  auto configSortBy = config_["sort-by"];
  if (configSortBy.isString()) {
    auto sortByStr = configSortBy.asString();
    try {
      m_sortBy = m_enumParser.parseStringToEnum(sortByStr, m_sortMap);
    } catch (const std::invalid_argument &e) {
      // Handle the case where the string is not a valid enum representation.
      m_sortBy = SortMethod::DEFAULT;
      g_warning("Invalid string representation for sort-by. Falling back to default sort method.");
    }
  }

  Json::Value ignoreWorkspaces = config["ignore-workspaces"];
  if (ignoreWorkspaces.isArray()) {
    for (Json::Value &workspaceRegex : ignoreWorkspaces) {
      if (workspaceRegex.isString()) {
        std::string ruleString = workspaceRegex.asString();
        try {
          const std::regex rule{ruleString, std::regex_constants::icase};
          m_ignoreWorkspaces.emplace_back(rule);
        } catch (const std::regex_error &e) {
          spdlog::error("Invalid rule {}: {}", ruleString, e.what());
        }
      } else {
        spdlog::error("Not a string: '{}'", workspaceRegex);
      }
    }
  }

  if (config_["persistent_workspaces"].isObject()) {
    spdlog::warn(
        "persistent_workspaces is deprecated. Please change config to use persistent-workspaces.");
  }

  if (config_["persistent-workspaces"].isObject() || config_["persistent_workspaces"].isObject()) {
    m_persistentWorkspaceConfig = config_["persistent-workspaces"].isObject()
                                      ? config_["persistent-workspaces"]
                                      : config_["persistent_workspaces"];
  }

  const Json::Value &formatWindowSeparator = config["format-window-separator"];
  m_formatWindowSeparator =
      formatWindowSeparator.isString() ? formatWindowSeparator.asString() : " ";

  const Json::Value &windowRewrite = config["window-rewrite"];
  if (!windowRewrite.isObject()) {
    spdlog::debug("window-rewrite is not defined or is not an object, using default rules.");
    return;
  }

  const Json::Value &windowRewriteDefaultConfig = config["window-rewrite-default"];
  std::string windowRewriteDefault =
      windowRewriteDefaultConfig.isString() ? windowRewriteDefaultConfig.asString() : "?";

  m_windowRewriteRules = util::RegexCollection(
      windowRewrite, windowRewriteDefault,
      [this](std::string &window_rule) { return windowRewritePriorityFunction(window_rule); });
}

void Workspaces::registerOrphanWindow(WindowCreationPayload create_window_payload) {
  if (!create_window_payload.isEmpty(*this)) {
    m_orphanWindowMap[create_window_payload.getAddress()] = create_window_payload.repr(*this);
  }
}

auto Workspaces::registerIpc() -> void {
  gIPC->registerForIPC("workspace", this);
  gIPC->registerForIPC("activespecial", this);
  gIPC->registerForIPC("createworkspace", this);
  gIPC->registerForIPC("destroyworkspace", this);
  gIPC->registerForIPC("focusedmon", this);
  gIPC->registerForIPC("moveworkspace", this);
  gIPC->registerForIPC("renameworkspace", this);
  gIPC->registerForIPC("openwindow", this);
  gIPC->registerForIPC("closewindow", this);
  gIPC->registerForIPC("movewindow", this);
  gIPC->registerForIPC("urgent", this);
  gIPC->registerForIPC("configreloaded", this);

  if (windowRewriteConfigUsesTitle()) {
    spdlog::info(
        "Registering for Hyprland's 'windowtitle' events because a user-defined window "
        "rewrite rule uses the 'title' field.");
    gIPC->registerForIPC("windowtitle", this);
  }
}

/**
 *  Workspaces::doUpdate - update workspaces in UI thread.
 *
 * Note: some memberfields are modified by both UI thread and event listener thread, use m_mutex to
 *       protect these member fields, and lock should released before calling AModule::update().
 */
void Workspaces::doUpdate() {
  std::unique_lock lock(m_mutex);

  // remove workspaces that wait to be removed
  for (auto &elem : m_workspacesToRemove) {
    removeWorkspace(elem);
  }
  m_workspacesToRemove.clear();

  // add workspaces that wait to be created
  for (auto &[workspaceData, clientsData] : m_workspacesToCreate) {
    createWorkspace(workspaceData, clientsData);
  }
  if (!m_workspacesToCreate.empty()) {
    updateWindowCount();
    sortWorkspaces();
  }
  m_workspacesToCreate.clear();

  // get all active workspaces
  spdlog::trace("Getting active workspaces");
  auto monitors = gIPC->getSocket1JsonReply("monitors");
  std::vector<std::string> visibleWorkspaces;
  for (Json::Value &monitor : monitors) {
    auto ws = monitor["activeWorkspace"];
    if (ws.isObject() && (ws["name"].isString())) {
      visibleWorkspaces.push_back(ws["name"].asString());
    }
    auto sws = monitor["specialWorkspace"];
    auto name = sws["name"].asString();
    if (sws.isObject() && (sws["name"].isString()) && !name.empty()) {
      visibleWorkspaces.push_back(!name.starts_with("special:") ? name : name.substr(8));
    }
  }

  spdlog::trace("Updating workspace states");
  auto updated_workspaces = gIPC->getSocket1JsonReply("workspaces");
  for (auto &workspace : m_workspaces) {
    // active
    workspace->setActive(workspace->name() == m_activeWorkspaceName ||
                         workspace->name() == m_activeSpecialWorkspaceName);
    // disable urgency if workspace is active
    if (workspace->name() == m_activeWorkspaceName && workspace->isUrgent()) {
      workspace->setUrgent(false);
    }

    // visible
    workspace->setVisible(std::find(visibleWorkspaces.begin(), visibleWorkspaces.end(),
                                    workspace->name()) != visibleWorkspaces.end());

    // set workspace icon
    std::string &workspaceIcon = m_iconsMap[""];
    if (m_withIcon) {
      spdlog::trace("Selecting icon for workspace {}", workspace->name());
      workspaceIcon = workspace->selectString(m_iconsMap);
    }

    // set tooltip
    std::string &workspaceTooltip = m_tooltipMap[""];
    if (m_withTooltip) {
      spdlog::trace("Selecting tooltip for workspace {}", workspace->name());
      workspaceTooltip = workspace->selectString(m_tooltipMap);
    }

    // update m_output
    auto updated_workspace =
        std::find_if(updated_workspaces.begin(), updated_workspaces.end(), [&workspace](auto &w) {
          auto wNameRaw = w["name"].asString();
          auto wName = wNameRaw.starts_with("special:") ? wNameRaw.substr(8) : wNameRaw;
          return wName == workspace->name();
        });

    if (updated_workspace != updated_workspaces.end()) {
      workspace->setOutput((*updated_workspace)["monitor"].asString());
    }

    workspace->update(m_format, workspaceIcon, workspaceTooltip);
  }

  spdlog::trace("Updating window count");
  bool anyWindowCreated = false;
  std::vector<WindowCreationPayload> notCreated;

  for (auto &windowPayload : m_windowsToCreate) {
    bool created = false;
    for (auto &workspace : m_workspaces) {
      if (workspace->onWindowOpened(windowPayload)) {
        created = true;
        anyWindowCreated = true;
        break;
      }
    }
    if (!created) {
      static auto const WINDOW_CREATION_TIMEOUT = 2;
      if (windowPayload.incrementTimeSpentUncreated() < WINDOW_CREATION_TIMEOUT) {
        notCreated.push_back(windowPayload);
      } else {
        registerOrphanWindow(windowPayload);
      }
    }
  }

  if (anyWindowCreated) {
    dp.emit();
  }

  m_windowsToCreate.clear();
  m_windowsToCreate = notCreated;
}

auto Workspaces::update() -> void {
  doUpdate();
  AModule::update();
}

bool isDoubleSpecial(std::string const &workspace_name) {
  // Hyprland's IPC sometimes reports the creation of workspaces strangely named
  // `special:special:<some_name>`. This function checks for that and is used
  // to avoid creating (and then removing) such workspaces.
  // See hyprwm/Hyprland#3424 for more info.
  return workspace_name.find("special:special:") != std::string::npos;
}

bool Workspaces::isWorkspaceIgnored(std::string const &name) {
  for (auto &rule : m_ignoreWorkspaces) {
    if (std::regex_match(name, rule)) {
      return true;
      break;
    }
  }

  return false;
}

void Workspaces::onEvent(const std::string &ev) {
  std::lock_guard<std::mutex> lock(m_mutex);
  std::string eventName(begin(ev), begin(ev) + ev.find_first_of('>'));
  std::string payload = ev.substr(eventName.size() + 2);

  if (eventName == "workspace") {
    onWorkspaceActivated(payload);
  } else if (eventName == "activespecial") {
    onSpecialWorkspaceActivated(payload);
  } else if (eventName == "destroyworkspace") {
    onWorkspaceDestroyed(payload);
  } else if (eventName == "createworkspace") {
    onWorkspaceCreated(payload);
  } else if (eventName == "focusedmon") {
    onMonitorFocused(payload);
  } else if (eventName == "moveworkspace") {
    onWorkspaceMoved(payload);
  } else if (eventName == "openwindow") {
    onWindowOpened(payload);
  } else if (eventName == "closewindow") {
    onWindowClosed(payload);
  } else if (eventName == "movewindow") {
    onWindowMoved(payload);
  } else if (eventName == "urgent") {
    setUrgentWorkspace(payload);
  } else if (eventName == "renameworkspace") {
    onWorkspaceRenamed(payload);
  } else if (eventName == "windowtitle") {
    onWindowTitleEvent(payload);
  } else if (eventName == "configreloaded") {
    onConfigReloaded();
  }

  dp.emit();
}

void Workspaces::onWorkspaceActivated(std::string const &payload) {
  m_activeWorkspaceName = payload;
}

void Workspaces::onSpecialWorkspaceActivated(std::string const &payload) {
  std::string name(begin(payload), begin(payload) + payload.find_first_of(','));
  m_activeSpecialWorkspaceName = (!name.starts_with("special:") ? name : name.substr(8));
}

void Workspaces::onWorkspaceDestroyed(std::string const &payload) {
  if (!isDoubleSpecial(payload)) {
    m_workspacesToRemove.push_back(payload);
  }
}

void Workspaces::onWorkspaceCreated(std::string const &workspaceName,
                                    Json::Value const &clientsData) {
  spdlog::debug("Workspace created: {}", workspaceName);
  auto const workspacesJson = gIPC->getSocket1JsonReply("workspaces");

  if (!isWorkspaceIgnored(workspaceName)) {
    auto const workspaceRules = gIPC->getSocket1JsonReply("workspacerules");
    for (Json::Value workspaceJson : workspacesJson) {
      std::string name = workspaceJson["name"].asString();
      if (name == workspaceName) {
        if ((allOutputs() || m_bar.output->name == workspaceJson["monitor"].asString()) &&
            (showSpecial() || !name.starts_with("special")) && !isDoubleSpecial(workspaceName)) {
          for (Json::Value const &rule : workspaceRules) {
            if (rule["workspaceString"].asString() == workspaceName) {
              workspaceJson["persistent-rule"] = rule["persistent"].asBool();
              break;
            }
          }

          m_workspacesToCreate.emplace_back(workspaceJson, clientsData);
          break;
        }
      } else {
        extendOrphans(workspaceJson["id"].asInt(), clientsData);
      }
    }
  } else {
    spdlog::trace("Not creating workspace because it is ignored: {}", workspaceName);
  }
}

void Workspaces::onWorkspaceMoved(std::string const &payload) {
  spdlog::debug("Workspace moved: {}", payload);

  // Update active workspace
  m_activeWorkspaceName = (gIPC->getSocket1JsonReply("activeworkspace"))["name"].asString();

  if (allOutputs()) return;

  std::string workspaceName = payload.substr(0, payload.find(','));
  std::string monitorName = payload.substr(payload.find(',') + 1);

  if (m_bar.output->name == monitorName) {
    Json::Value clientsData = gIPC->getSocket1JsonReply("clients");
    onWorkspaceCreated(workspaceName, clientsData);
  } else {
    spdlog::debug("Removing workspace because it was moved to another monitor: {}");
    onWorkspaceDestroyed(workspaceName);
  }
}

void Workspaces::onWorkspaceRenamed(std::string const &payload) {
  spdlog::debug("Workspace renamed: {}", payload);
  std::string workspaceIdStr = payload.substr(0, payload.find(','));
  int workspaceId = workspaceIdStr == "special" ? -99 : std::stoi(workspaceIdStr);
  std::string newName = payload.substr(payload.find(',') + 1);
  for (auto &workspace : m_workspaces) {
    if (workspace->id() == workspaceId) {
      if (workspace->name() == m_activeWorkspaceName) {
        m_activeWorkspaceName = newName;
      }
      workspace->setName(newName);
      break;
    }
  }
  sortWorkspaces();
}

void Workspaces::onMonitorFocused(std::string const &payload) {
  spdlog::trace("Monitor focused: {}", payload);
  m_activeWorkspaceName = payload.substr(payload.find(',') + 1);

  for (Json::Value &monitor : gIPC->getSocket1JsonReply("monitors")) {
    if (monitor["name"].asString() == payload.substr(0, payload.find(','))) {
      auto name = monitor["specialWorkspace"]["name"].asString();
      m_activeSpecialWorkspaceName = !name.starts_with("special:") ? name : name.substr(8);
    }
  }
}

void Workspaces::onWindowOpened(std::string const &payload) {
  spdlog::trace("Window opened: {}", payload);
  updateWindowCount();
  size_t lastCommaIdx = 0;
  size_t nextCommaIdx = payload.find(',');
  std::string windowAddress = payload.substr(lastCommaIdx, nextCommaIdx - lastCommaIdx);

  lastCommaIdx = nextCommaIdx;
  nextCommaIdx = payload.find(',', nextCommaIdx + 1);
  std::string workspaceName = payload.substr(lastCommaIdx + 1, nextCommaIdx - lastCommaIdx - 1);

  lastCommaIdx = nextCommaIdx;
  nextCommaIdx = payload.find(',', nextCommaIdx + 1);
  std::string windowClass = payload.substr(lastCommaIdx + 1, nextCommaIdx - lastCommaIdx - 1);

  std::string windowTitle = payload.substr(nextCommaIdx + 1, payload.length() - nextCommaIdx);

  m_windowsToCreate.emplace_back(workspaceName, windowAddress, windowClass, windowTitle);
}

void Workspaces::onWindowClosed(std::string const &addr) {
  spdlog::trace("Window closed: {}", addr);
  updateWindowCount();
  for (auto &workspace : m_workspaces) {
    if (workspace->closeWindow(addr)) {
      break;
    }
  }
}

void Workspaces::onWindowMoved(std::string const &payload) {
  spdlog::trace("Window moved: {}", payload);
  updateWindowCount();
  size_t lastCommaIdx = 0;
  size_t nextCommaIdx = payload.find(',');
  std::string windowAddress = payload.substr(lastCommaIdx, nextCommaIdx - lastCommaIdx);

  std::string workspaceName = payload.substr(nextCommaIdx + 1, payload.length() - nextCommaIdx);

  std::string windowRepr;

  // If the window was still queued to be created, just change its destination
  // and exit
  for (auto &window : m_windowsToCreate) {
    if (window.getAddress() == windowAddress) {
      window.moveToWorksace(workspaceName);
      return;
    }
  }

  // Take the window's representation from the old workspace...
  for (auto &workspace : m_workspaces) {
    if (auto windowAddr = workspace->closeWindow(windowAddress); windowAddr != std::nullopt) {
      windowRepr = windowAddr.value();
      break;
    }
  }

  // ...if it was empty, check if the window is an orphan...
  if (windowRepr.empty() && m_orphanWindowMap.contains(windowAddress)) {
    windowRepr = m_orphanWindowMap[windowAddress];
  }

  // ...and then add it to the new workspace
  if (!windowRepr.empty()) {
    m_windowsToCreate.emplace_back(workspaceName, windowAddress, windowRepr);
  }
}

void Workspaces::onWindowTitleEvent(std::string const &payload) {
  spdlog::trace("Window title changed: {}", payload);
  std::optional<std::function<void(WindowCreationPayload)>> inserter;

  // If the window was an orphan, rename it at the orphan's vector
  if (m_orphanWindowMap.contains(payload)) {
    inserter = [this](WindowCreationPayload wcp) { this->registerOrphanWindow(std::move(wcp)); };
  } else {
    auto windowWorkspace =
        std::find_if(m_workspaces.begin(), m_workspaces.end(),
                     [payload](auto &workspace) { return workspace->containsWindow(payload); });

    // If the window exists on a workspace, rename it at the workspace's window
    // map
    if (windowWorkspace != m_workspaces.end()) {
      inserter = [windowWorkspace](WindowCreationPayload wcp) {
        (*windowWorkspace)->insertWindow(std::move(wcp));
      };
    } else {
      auto queuedWindow = std::find_if(
          m_windowsToCreate.begin(), m_windowsToCreate.end(),
          [payload](auto &windowPayload) { return windowPayload.getAddress() == payload; });

      // If the window was queued, rename it in the queue
      if (queuedWindow != m_windowsToCreate.end()) {
        inserter = [queuedWindow](WindowCreationPayload wcp) { *queuedWindow = std::move(wcp); };
      }
    }
  }

  if (inserter.has_value()) {
    Json::Value clientsData = gIPC->getSocket1JsonReply("clients");
    std::string jsonWindowAddress = fmt::format("0x{}", payload);

    auto client =
        std::find_if(clientsData.begin(), clientsData.end(), [jsonWindowAddress](auto &client) {
          return client["address"].asString() == jsonWindowAddress;
        });

    if (!client->empty()) {
      (*inserter)({*client});
    }
  }
}

void Workspaces::onConfigReloaded() {
  spdlog::info("Hyprland config reloaded, reinitializing hyprland/workspaces module...");
  init();
}

void Workspaces::updateWindowCount() {
  const Json::Value workspacesJson = gIPC->getSocket1JsonReply("workspaces");
  for (auto &workspace : m_workspaces) {
    auto workspaceJson =
        std::find_if(workspacesJson.begin(), workspacesJson.end(), [&](Json::Value const &x) {
          return x["name"].asString() == workspace->name() ||
                 (workspace->isSpecial() && x["name"].asString() == "special:" + workspace->name());
        });
    uint32_t count = 0;
    if (workspaceJson != workspacesJson.end()) {
      try {
        count = (*workspaceJson)["windows"].asUInt();
      } catch (const std::exception &e) {
        spdlog::error("Failed to update window count: {}", e.what());
      }
    }
    workspace->setWindows(count);
  }
}

void Workspace::initializeWindowMap(const Json::Value &clients_data) {
  m_windowMap.clear();
  for (auto client : clients_data) {
    if (client["workspace"]["id"].asInt() == id()) {
      insertWindow({client});
    }
  }
}

void Workspace::insertWindow(WindowCreationPayload create_window_paylod) {
  if (!create_window_paylod.isEmpty(m_workspaceManager)) {
    m_windowMap[create_window_paylod.getAddress()] = create_window_paylod.repr(m_workspaceManager);
  }
};

std::string Workspace::removeWindow(WindowAddress const &addr) {
  std::string windowRepr = m_windowMap[addr];
  m_windowMap.erase(addr);
  return windowRepr;
}

bool Workspace::onWindowOpened(WindowCreationPayload const &create_window_paylod) {
  if (create_window_paylod.getWorkspaceName() == name()) {
    insertWindow(create_window_paylod);
    return true;
  }
  return false;
}

std::optional<std::string> Workspace::closeWindow(WindowAddress const &addr) {
  if (m_windowMap.contains(addr)) {
    return removeWindow(addr);
  }
  return std::nullopt;
}

void Workspaces::createWorkspace(Json::Value const &workspace_data,
                                 Json::Value const &clients_data) {
  auto workspaceName = workspace_data["name"].asString();
  spdlog::debug("Creating workspace {}", workspaceName);

  // avoid recreating existing workspaces
  auto workspace = std::find_if(
      m_workspaces.begin(), m_workspaces.end(),
      [workspaceName](std::unique_ptr<Workspace> const &w) {
        return (workspaceName.starts_with("special:") && workspaceName.substr(8) == w->name()) ||
               workspaceName == w->name();
      });

  if (workspace != m_workspaces.end()) {
    // don't recreate workspace, but update persistency if necessary
    const auto keys = workspace_data.getMemberNames();

    const auto *k = "persistent-rule";
    if (std::find(keys.begin(), keys.end(), k) != keys.end()) {
      spdlog::debug("Set dynamic persistency of workspace {} to: {}", workspaceName,
                    workspace_data[k].asBool() ? "true" : "false");
      (*workspace)->setPersistentRule(workspace_data[k].asBool());
    }

    k = "persistent-config";
    if (std::find(keys.begin(), keys.end(), k) != keys.end()) {
      spdlog::debug("Set config persistency of workspace {} to: {}", workspaceName,
                    workspace_data[k].asBool() ? "true" : "false");
      (*workspace)->setPersistentConfig(workspace_data[k].asBool());
    }

    return;
  }

  // create new workspace
  m_workspaces.emplace_back(std::make_unique<Workspace>(workspace_data, *this, clients_data));
  Gtk::Button &newWorkspaceButton = m_workspaces.back()->button();
  m_box.pack_start(newWorkspaceButton, false, false);
  sortWorkspaces();
  newWorkspaceButton.show_all();
}

void Workspaces::removeWorkspace(std::string const &name) {
  spdlog::debug("Removing workspace {}", name);
  auto workspace =
      std::find_if(m_workspaces.begin(), m_workspaces.end(), [&](std::unique_ptr<Workspace> &x) {
        return (name.starts_with("special:") && name.substr(8) == x->name()) || name == x->name();
      });

  if (workspace == m_workspaces.end()) {
    // happens when a workspace on another monitor is destroyed
    return;
  }

  if ((*workspace)->isPersistentConfig()) {
    spdlog::trace("Not removing config persistent workspace {}", name);
    return;
  }

  m_box.remove(workspace->get()->button());
  m_workspaces.erase(workspace);
}

Json::Value createMonitorWorkspaceData(std::string const &name, std::string const &monitor) {
  spdlog::trace("Creating persistent workspace: {} on monitor {}", name, monitor);
  Json::Value workspaceData;
  try {
    // numbered persistent workspaces get the name as ID
    workspaceData["id"] = name == "special" ? -99 : std::stoi(name);
  } catch (const std::exception &e) {
    // named persistent workspaces start with ID=0
    workspaceData["id"] = 0;
  }
  workspaceData["name"] = name;
  workspaceData["monitor"] = monitor;
  workspaceData["windows"] = 0;
  return workspaceData;
}

void Workspaces::loadPersistentWorkspacesFromConfig(Json::Value const &clientsJson) {
  spdlog::info("Loading persistent workspaces from Waybar config");
  const std::vector<std::string> keys = m_persistentWorkspaceConfig.getMemberNames();
  std::vector<std::string> persistentWorkspacesToCreate;

  const std::string currentMonitor = m_bar.output->name;
  const bool monitorInConfig = std::find(keys.begin(), keys.end(), currentMonitor) != keys.end();
  for (const std::string &key : keys) {
    // only add if either:
    // 1. key is the current monitor name
    // 2. key is "*" and this monitor is not already defined in the config
    bool canCreate = key == currentMonitor || (key == "*" && !monitorInConfig);
    const Json::Value &value = m_persistentWorkspaceConfig[key];
    spdlog::trace("Parsing persistent workspace config: {} => {}", key, value.toStyledString());

    if (value.isInt()) {
      // value is a number => create that many workspaces for this monitor
      if (canCreate) {
        int amount = value.asInt();
        spdlog::debug("Creating {} persistent workspaces for monitor {}", amount, currentMonitor);
        for (int i = 0; i < amount; i++) {
          persistentWorkspacesToCreate.emplace_back(std::to_string(m_monitorId * amount + i + 1));
        }
      }
    } else if (value.isArray() && !value.empty()) {
      // value is an array => create defined workspaces for this monitor
      if (canCreate) {
        for (const Json::Value &workspace : value) {
          if (workspace.isInt()) {
            spdlog::debug("Creating workspace {} on monitor {}", workspace, currentMonitor);
            persistentWorkspacesToCreate.emplace_back(std::to_string(workspace.asInt()));
          }
        }
      } else {
        // key is the workspace and value is array of monitors to create on
        for (const Json::Value &monitor : value) {
          if (monitor.isString() && monitor.asString() == currentMonitor) {
            persistentWorkspacesToCreate.emplace_back(currentMonitor);
            break;
          }
        }
      }
    } else {
      // this workspace should be displayed on all monitors
      persistentWorkspacesToCreate.emplace_back(key);
    }
  }

  for (auto const &workspace : persistentWorkspacesToCreate) {
    auto workspaceData = createMonitorWorkspaceData(workspace, m_bar.output->name);
    workspaceData["persistent-config"] = true;
    m_workspacesToCreate.emplace_back(workspaceData, clientsJson);
  }
}

void Workspaces::loadPersistentWorkspacesFromWorkspaceRules(const Json::Value &clientsJson) {
  spdlog::info("Loading persistent workspaces from Hyprland workspace rules");

  auto const workspaceRules = gIPC->getSocket1JsonReply("workspacerules");
  for (Json::Value const &rule : workspaceRules) {
    if (!rule["workspaceString"].isString()) {
      spdlog::warn("Workspace rules: invalid workspaceString, skipping: {}", rule);
      continue;
    }
    if (!rule["persistent"].asBool()) {
      continue;
    }
    auto const &workspace = rule["workspaceString"].asString();
    auto const &monitor = rule["monitor"].asString();
    // create this workspace persistently if:
    // 1. the allOutputs config option is enabled
    // 2. the rule's monitor is the current monitor
    // 3. no monitor is specified in the rule => assume it needs to be persistent on every monitor
    if (allOutputs() || m_bar.output->name == monitor || monitor.empty()) {
      // => persistent workspace should be shown on this monitor
      auto workspaceData = createMonitorWorkspaceData(workspace, m_bar.output->name);
      workspaceData["persistent-rule"] = true;
      m_workspacesToCreate.emplace_back(workspaceData, clientsJson);
    } else {
      m_workspacesToRemove.emplace_back(workspace);
    }
  }
}

void Workspaces::setCurrentMonitorId() {
  // get monitor ID from name (used by persistent workspaces)
  m_monitorId = 0;
  auto monitors = gIPC->getSocket1JsonReply("monitors");
  auto currentMonitor = std::find_if(
      monitors.begin(), monitors.end(),
      [this](const Json::Value &m) { return m["name"].asString() == m_bar.output->name; });
  if (currentMonitor == monitors.end()) {
    spdlog::error("Monitor '{}' does not have an ID? Using 0", m_bar.output->name);
  } else {
    m_monitorId = (*currentMonitor)["id"].asInt();
    spdlog::trace("Current monitor ID: {}", m_monitorId);
  }
}

void Workspaces::initializeWorkspaces() {
  spdlog::debug("Initializing workspaces");

  // if the workspace rules changed since last initialization, make sure we reset everything:
  for (auto &workspace : m_workspaces) {
    m_workspacesToRemove.push_back(workspace->name());
  }

  // get all current workspaces
  auto const workspacesJson = gIPC->getSocket1JsonReply("workspaces");
  auto const clientsJson = gIPC->getSocket1JsonReply("clients");

  for (Json::Value workspaceJson : workspacesJson) {
    std::string workspaceName = workspaceJson["name"].asString();
    if ((allOutputs() || m_bar.output->name == workspaceJson["monitor"].asString()) &&
        (!workspaceName.starts_with("special") || showSpecial()) &&
        !isWorkspaceIgnored(workspaceName)) {
      m_workspacesToCreate.emplace_back(workspaceJson, clientsJson);
    } else {
      extendOrphans(workspaceJson["id"].asInt(), clientsJson);
    }
  }

  spdlog::debug("Initializing persistent workspaces");
  if (m_persistentWorkspaceConfig.isObject()) {
    // a persistent workspace config is defined, so use that instead of workspace rules
    loadPersistentWorkspacesFromConfig(clientsJson);
  }
  // load Hyprland's workspace rules
  loadPersistentWorkspacesFromWorkspaceRules(clientsJson);
}

void Workspaces::extendOrphans(int workspaceId, Json::Value const &clientsJson) {
  spdlog::trace("Extending orphans with workspace {}", workspaceId);
  for (const auto &client : clientsJson) {
    if (client["workspace"]["id"].asInt() == workspaceId) {
      registerOrphanWindow({client});
    }
  }
}

void Workspaces::init() {
  m_activeWorkspaceName = (gIPC->getSocket1JsonReply("activeworkspace"))["name"].asString();

  initializeWorkspaces();
  dp.emit();
}

Workspaces::~Workspaces() {
  gIPC->unregisterForIPC(this);
  // wait for possible event handler to finish
  std::lock_guard<std::mutex> lg(m_mutex);
}

Workspace::Workspace(const Json::Value &workspace_data, Workspaces &workspace_manager,
                     const Json::Value &clients_data)
    : m_workspaceManager(workspace_manager),
      m_id(workspace_data["id"].asInt()),
      m_name(workspace_data["name"].asString()),
      m_output(workspace_data["monitor"].asString()),  // TODO:allow using monitor desc
      m_windows(workspace_data["windows"].asInt()),
      m_isActive(true),
      m_isPersistentRule(workspace_data["persistent-rule"].asBool()),
      m_isPersistentConfig(workspace_data["persistent-config"].asBool()) {
  if (m_name.starts_with("name:")) {
    m_name = m_name.substr(5);
  } else if (m_name.starts_with("special")) {
    m_name = m_id == -99 ? m_name : m_name.substr(8);
    m_isSpecial = true;
  }

  m_button.add_events(Gdk::BUTTON_PRESS_MASK);
  m_button.signal_button_press_event().connect(sigc::mem_fun(*this, &Workspace::handleClicked),
                                               false);

  m_button.set_relief(Gtk::RELIEF_NONE);
  m_content.set_center_widget(m_label);
  m_button.add(m_content);

  initializeWindowMap(clients_data);
}

void addOrRemoveClass(const Glib::RefPtr<Gtk::StyleContext> &context, bool condition,
                      const std::string &class_name) {
  if (condition) {
    context->add_class(class_name);
  } else {
    context->remove_class(class_name);
  }
}

void Workspace::update(const std::string &format, const std::string &icon, const std::string &tooltipFormat) {
  // clang-format off
  if (this->m_workspaceManager.activeOnly() && \
     !this->isActive() && \
     !this->isPersistent() && \
     !this->isVisible() && \
     !this->isSpecial()) {
    // clang-format on
    // if activeOnly is true, hide if not active, persistent, visible or special
    m_button.hide();
    return;
  }
  m_button.show();

  auto styleContext = m_button.get_style_context();
  addOrRemoveClass(styleContext, isActive(), "active");
  addOrRemoveClass(styleContext, isSpecial(), "special");
  addOrRemoveClass(styleContext, isEmpty(), "empty");
  addOrRemoveClass(styleContext, isPersistent(), "persistent");
  addOrRemoveClass(styleContext, isUrgent(), "urgent");
  addOrRemoveClass(styleContext, isVisible(), "visible");
  addOrRemoveClass(styleContext, m_workspaceManager.getBarOutput() == output(), "hosting-monitor");

  std::string windows;
  auto windowSeparator = m_workspaceManager.getWindowSeparator();

  bool isNotFirst = false;

  for (auto &[_pid, window_repr] : m_windowMap) {
    if (isNotFirst) {
      windows.append(windowSeparator);
    }
    isNotFirst = true;
    windows.append(window_repr);
  }

  m_button.set_tooltip_text(fmt::format(fmt::runtime(tooltipFormat), fmt::arg("id", id()),
                                 fmt::arg("name", name()), fmt::arg("icon", icon),
                                 fmt::arg("windows", windows)));

  m_label.set_markup(fmt::format(fmt::runtime(format), fmt::arg("id", id()),
                                 fmt::arg("name", name()), fmt::arg("icon", icon),
                                 fmt::arg("windows", windows)));
}

void Workspaces::sortWorkspaces() {
  std::sort(m_workspaces.begin(), m_workspaces.end(),
            [&](std::unique_ptr<Workspace> &a, std::unique_ptr<Workspace> &b) {
              // Helper comparisons
              auto isIdLess = a->id() < b->id();
              auto isNameLess = a->name() < b->name();

              switch (m_sortBy) {
                case SortMethod::ID:
                  return isIdLess;
                case SortMethod::NAME:
                  return isNameLess;
                case SortMethod::NUMBER:
                  try {
                    return std::stoi(a->name()) < std::stoi(b->name());
                  } catch (const std::invalid_argument &) {
                    // Handle the exception if necessary.
                    break;
                  }
                case SortMethod::DEFAULT:
                default:
                  // Handle the default case here.
                  // normal -> named persistent -> named -> special -> named special

                  // both normal (includes numbered persistent) => sort by ID
                  if (a->id() > 0 && b->id() > 0) {
                    return isIdLess;
                  }

                  // one normal, one special => normal first
                  if ((a->isSpecial()) ^ (b->isSpecial())) {
                    return b->isSpecial();
                  }

                  // only one normal, one named
                  if ((a->id() > 0) ^ (b->id() > 0)) {
                    return a->id() > 0;
                  }

                  // both special
                  if (a->isSpecial() && b->isSpecial()) {
                    // if one is -99 => put it last
                    if (a->id() == -99 || b->id() == -99) {
                      return b->id() == -99;
                    }
                    // both are 0 (not yet named persistents) / named specials (-98 <= ID <= -1)
                    return isNameLess;
                  }

                  // sort non-special named workspaces by name (ID <= -1377)
                  return isNameLess;
                  break;
              }

              // Return a default value if none of the cases match.
              return isNameLess;  // You can adjust this to your specific needs.
            });

  for (size_t i = 0; i < m_workspaces.size(); ++i) {
    m_box.reorder_child(m_workspaces[i]->button(), i);
  }
}

std::string &Workspace::selectString(std::map<std::string, std::string> &string_map) {
  if (isUrgent()) {
    auto urgentStringIt = string_map.find("urgent");
    if (urgentStringIt != string_map.end()) {
      return urgentStringIt->second;
    }
  }

  if (isActive()) {
    auto activeStringIt = string_map.find("active");
    if (activeStringIt != string_map.end()) {
      return activeStringIt->second;
    }
  }

  if (isSpecial()) {
    auto specialStringIt = string_map.find("special");
    if (specialStringIt != string_map.end()) {
      return specialStringIt->second;
    }
  }

  auto namedStringIt = string_map.find(name());
  if (namedStringIt != string_map.end()) {
    return namedStringIt->second;
  }

  if (isVisible()) {
    auto visibleStringIt = string_map.find("visible");
    if (visibleStringIt != string_map.end()) {
      return visibleStringIt->second;
    }
  }

  if (isEmpty()) {
    auto emptyStringIt = string_map.find("empty");
    if (emptyStringIt != string_map.end()) {
      return emptyStringIt->second;
    }
  }

  if (isPersistent()) {
    auto persistentStringIt = string_map.find("persistent");
    if (persistentStringIt != string_map.end()) {
      return persistentStringIt->second;
    }
  }

  auto defaultStringIt = string_map.find("default");
  if (defaultStringIt != string_map.end()) {
    return defaultStringIt->second;
  }

  return m_name;
}

bool Workspace::handleClicked(GdkEventButton *bt) const {
  if (bt->type == GDK_BUTTON_PRESS) {
    try {
      if (id() > 0) {  // normal
        if (m_workspaceManager.moveToMonitor()) {
          gIPC->getSocket1Reply("dispatch focusworkspaceoncurrentmonitor " + std::to_string(id()));
        } else {
          gIPC->getSocket1Reply("dispatch workspace " + std::to_string(id()));
        }
      } else if (!isSpecial()) {  // named (this includes persistent)
        if (m_workspaceManager.moveToMonitor()) {
          gIPC->getSocket1Reply("dispatch focusworkspaceoncurrentmonitor name:" + name());
        } else {
          gIPC->getSocket1Reply("dispatch workspace name:" + name());
        }
      } else if (id() != -99) {  // named special
        gIPC->getSocket1Reply("dispatch togglespecialworkspace " + name());
      } else {  // special
        gIPC->getSocket1Reply("dispatch togglespecialworkspace");
      }
      return true;
    } catch (const std::exception &e) {
      spdlog::error("Failed to dispatch workspace: {}", e.what());
    }
  }
  return false;
}

void Workspaces::setUrgentWorkspace(std::string const &windowaddress) {
  const Json::Value clientsJson = gIPC->getSocket1JsonReply("clients");
  int workspaceId = -1;

  for (Json::Value clientJson : clientsJson) {
    if (clientJson["address"].asString().ends_with(windowaddress)) {
      workspaceId = clientJson["workspace"]["id"].asInt();
      break;
    }
  }

  auto workspace =
      std::find_if(m_workspaces.begin(), m_workspaces.end(),
                   [workspaceId](std::unique_ptr<Workspace> &x) { return x->id() == workspaceId; });
  if (workspace != m_workspaces.end()) {
    workspace->get()->setUrgent();
  }
}

std::string Workspaces::getRewrite(std::string window_class, std::string window_title) {
  std::string windowReprKey;
  if (windowRewriteConfigUsesTitle()) {
    windowReprKey = fmt::format("class<{}> title<{}>", window_class, window_title);
  } else {
    windowReprKey = fmt::format("class<{}>", window_class);
  }
  auto const rewriteRule = m_windowRewriteRules.get(windowReprKey);
  return fmt::format(fmt::runtime(rewriteRule), fmt::arg("class", window_class),
                     fmt::arg("title", window_title));
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

WindowCreationPayload::WindowCreationPayload(Json::Value const &client_data)
    : m_window(std::make_pair(client_data["class"].asString(), client_data["title"].asString())),
      m_windowAddress(client_data["address"].asString()),
      m_workspaceName(client_data["workspace"]["name"].asString()) {
  clearAddr();
  clearWorkspaceName();
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

void WindowCreationPayload::moveToWorksace(std::string &new_workspace_name) {
  m_workspaceName = new_workspace_name;
}

}  // namespace waybar::modules::hyprland
