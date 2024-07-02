#include "modules/hyprland/workspaces.hpp"

#include <json/value.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "util/regex_collection.hpp"

namespace waybar::modules::hyprland {

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

Workspaces::~Workspaces() {
  gIPC->unregisterForIPC(this);
  // wait for possible event handler to finish
  std::lock_guard<std::mutex> lg(m_mutex);
}

void Workspaces::init() {
  m_activeWorkspaceName = (gIPC->getSocket1JsonReply("activeworkspace"))["name"].asString();

  initializeWorkspaces();
  dp.emit();
}

Json::Value Workspaces::createMonitorWorkspaceData(std::string const &name,
                                                   std::string const &monitor) {
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

void Workspaces::createWorkspacesToCreate() {
  for (const auto &[workspaceData, clientsData] : m_workspacesToCreate) {
    createWorkspace(workspaceData, clientsData);
  }
  if (!m_workspacesToCreate.empty()) {
    updateWindowCount();
    sortWorkspaces();
  }
  m_workspacesToCreate.clear();
}

/**
 *  Workspaces::doUpdate - update workspaces in UI thread.
 *
 * Note: some memberfields are modified by both UI thread and event listener thread, use m_mutex to
 *       protect these member fields, and lock should released before calling AModule::update().
 */
void Workspaces::doUpdate() {
  std::unique_lock lock(m_mutex);

  removeWorkspacesToRemove();
  createWorkspacesToCreate();
  updateWorkspaceStates();
  updateWindowCount();
  sortWorkspaces();

  bool anyWindowCreated = updateWindowsToCreate();

  if (anyWindowCreated) {
    dp.emit();
  }
}

void Workspaces::extendOrphans(int workspaceId, Json::Value const &clientsJson) {
  spdlog::trace("Extending orphans with workspace {}", workspaceId);
  for (const auto &client : clientsJson) {
    if (client["workspace"]["id"].asInt() == workspaceId) {
      registerOrphanWindow({client});
    }
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

std::vector<std::string> Workspaces::getVisibleWorkspaces() {
  std::vector<std::string> visibleWorkspaces;
  auto monitors = gIPC->getSocket1JsonReply("monitors");
  for (const auto &monitor : monitors) {
    auto ws = monitor["activeWorkspace"];
    if (ws.isObject() && ws["name"].isString()) {
      visibleWorkspaces.push_back(ws["name"].asString());
    }
    auto sws = monitor["specialWorkspace"];
    auto name = sws["name"].asString();
    if (sws.isObject() && sws["name"].isString() && !name.empty()) {
      visibleWorkspaces.push_back(!name.starts_with("special:") ? name : name.substr(8));
    }
  }
  return visibleWorkspaces;
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

auto Workspaces::parseConfig(const Json::Value &config) -> void {
  const auto &configFormat = config["format"];
  m_format = configFormat.isString() ? configFormat.asString() : "{name}";
  m_withIcon = m_format.find("{icon}") != std::string::npos;

  if (m_withIcon && m_iconsMap.empty()) {
    populateIconsMap(config["format-icons"]);
  }

  populateBoolConfig(config, "all-outputs", m_allOutputs);
  populateBoolConfig(config, "show-special", m_showSpecial);
  populateBoolConfig(config, "special-visible-only", m_specialVisibleOnly);
  populateBoolConfig(config, "active-only", m_activeOnly);
  populateBoolConfig(config, "move-to-monitor", m_moveToMonitor);

  m_persistentWorkspaceConfig = config.get("persistent-workspaces", Json::Value());
  populateSortByConfig(config);
  populateIgnoreWorkspacesConfig(config);
  populateFormatWindowSeparatorConfig(config);
  populateWindowRewriteConfig(config);
}

auto Workspaces::populateIconsMap(const Json::Value &formatIcons) -> void {
  for (const auto &name : formatIcons.getMemberNames()) {
    m_iconsMap.emplace(name, formatIcons[name].asString());
  }
  m_iconsMap.emplace("", "");
}

auto Workspaces::populateBoolConfig(const Json::Value &config, const std::string &key, bool &member)
    -> void {
  const auto &configValue = config[key];
  if (configValue.isBool()) {
    member = configValue.asBool();
  }
}

auto Workspaces::populateSortByConfig(const Json::Value &config) -> void {
  const auto &configSortBy = config["sort-by"];
  if (configSortBy.isString()) {
    auto sortByStr = configSortBy.asString();
    try {
      m_sortBy = m_enumParser.parseStringToEnum(sortByStr, m_sortMap);
    } catch (const std::invalid_argument &e) {
      m_sortBy = SortMethod::DEFAULT;
      spdlog::warn(
          "Invalid string representation for sort-by. Falling back to default sort method.");
    }
  }
}

auto Workspaces::populateIgnoreWorkspacesConfig(const Json::Value &config) -> void {
  auto ignoreWorkspaces = config["ignore-workspaces"];
  if (ignoreWorkspaces.isArray()) {
    for (const auto &workspaceRegex : ignoreWorkspaces) {
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
}

auto Workspaces::populateFormatWindowSeparatorConfig(const Json::Value &config) -> void {
  const auto &formatWindowSeparator = config["format-window-separator"];
  m_formatWindowSeparator =
      formatWindowSeparator.isString() ? formatWindowSeparator.asString() : " ";
}

auto Workspaces::populateWindowRewriteConfig(const Json::Value &config) -> void {
  const auto &windowRewrite = config["window-rewrite"];
  if (!windowRewrite.isObject()) {
    spdlog::debug("window-rewrite is not defined or is not an object, using default rules.");
    return;
  }

  const auto &windowRewriteDefaultConfig = config["window-rewrite-default"];
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

void Workspaces::removeWorkspacesToRemove() {
  for (const auto &workspaceName : m_workspacesToRemove) {
    removeWorkspace(workspaceName);
  }
  m_workspacesToRemove.clear();
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

auto Workspaces::update() -> void {
  doUpdate();
  AModule::update();
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

bool Workspaces::updateWindowsToCreate() {
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
  m_windowsToCreate.clear();
  m_windowsToCreate = notCreated;
  return anyWindowCreated;
}

void Workspaces::updateWorkspaceStates() {
  const std::vector<std::string> visibleWorkspaces = getVisibleWorkspaces();
  auto updatedWorkspaces = gIPC->getSocket1JsonReply("workspaces");
  for (auto &workspace : m_workspaces) {
    workspace->setActive(workspace->name() == m_activeWorkspaceName ||
                         workspace->name() == m_activeSpecialWorkspaceName);
    if (workspace->name() == m_activeWorkspaceName && workspace->isUrgent()) {
      workspace->setUrgent(false);
    }
    workspace->setVisible(std::find(visibleWorkspaces.begin(), visibleWorkspaces.end(),
                                    workspace->name()) != visibleWorkspaces.end());
    std::string &workspaceIcon = m_iconsMap[""];
    if (m_withIcon) {
      workspaceIcon = workspace->selectIcon(m_iconsMap);
    }
    auto updatedWorkspace = std::find_if(
        updatedWorkspaces.begin(), updatedWorkspaces.end(), [&workspace](const auto &w) {
          auto wNameRaw = w["name"].asString();
          auto wName = wNameRaw.starts_with("special:") ? wNameRaw.substr(8) : wNameRaw;
          return wName == workspace->name();
        });
    if (updatedWorkspace != updatedWorkspaces.end()) {
      workspace->setOutput((*updatedWorkspace)["monitor"].asString());
    }
    workspace->update(m_format, workspaceIcon);
  }
}

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

}  // namespace waybar::modules::hyprland
