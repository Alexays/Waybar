#include "modules/hyprland/workspaces.hpp"

#include <json/value.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <charconv>
#include <memory>
#include <shared_mutex>
#include <string>
#include <thread>
#include <utility>
#include <variant>

#include "util/regex_collection.hpp"

namespace waybar::modules::hyprland {

std::shared_mutex workspaceCreateSmtx;

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
    : AModule(config, "workspaces", id, false, false),
      m_bar(bar),
      m_box(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0) {
  modulesReady = true;
  parseConfig(config);

  m_box.set_name("workspaces");
  if (!id.empty()) {
    m_box.get_style_context()->add_class(id);
  }
  event_box_.add(m_box);

  if (!gIPC) {
    gIPC = std::make_unique<IPC>();
  }

  init();
  registerIpc();
}

auto Workspaces::parseConfig(const Json::Value &config) -> void {
  const Json::Value &configFormat = config["format"];

  m_format = configFormat.isString() ? configFormat.asString() : "{name}";
  m_withIcon = m_format.find("{icon}") != std::string::npos;

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

  const Json::Value &formatWindowSeparator = config["format-window-separator"];
  m_formatWindowSeparator =
      formatWindowSeparator.isString() ? formatWindowSeparator.asString() : " ";

  const Json::Value &windowRewrite = config["window-rewrite"];

  const Json::Value &windowRewriteDefaultConfig = config["window-rewrite-default"];
  std::string windowRewriteDefault =
      windowRewriteDefaultConfig.isString() ? windowRewriteDefaultConfig.asString() : "?";

  m_windowRewriteRules = util::RegexCollection(
      windowRewrite, windowRewriteDefault,
      [this](std::string &window_rule) { return windowRewritePriorityFunction(window_rule); });
}

auto Workspaces::registerIpc() -> void {
  gIPC->registerForIPC("workspace", this);
  gIPC->registerForIPC("createworkspace", this);
  gIPC->registerForIPC("destroyworkspace", this);
  gIPC->registerForIPC("focusedmon", this);
  gIPC->registerForIPC("moveworkspace", this);
  gIPC->registerForIPC("renameworkspace", this);
  gIPC->registerForIPC("openwindow", this);
  gIPC->registerForIPC("closewindow", this);
  gIPC->registerForIPC("movewindow", this);
  gIPC->registerForIPC("urgent", this);

  if (windowRewriteConfigUsesTitle()) {
    spdlog::info(
        "Registering for Hyprland's 'windowtitle' events because a user-defined window "
        "rewrite rule uses the 'title' field.");
    gIPC->registerForIPC("windowtitle", this);
  }
}

auto Workspaces::update() -> void {
  // remove workspaces that wait to be removed
  unsigned int currentRemoveWorkspaceNum = 0;
  for (const std::string &workspaceToRemove : m_workspacesToRemove) {
    removeWorkspace(workspaceToRemove);
    currentRemoveWorkspaceNum++;
  }
  for (unsigned int i = 0; i < currentRemoveWorkspaceNum; i++) {
    m_workspacesToRemove.erase(m_workspacesToRemove.begin());
  }

  // add workspaces that wait to be created
  std::shared_lock<std::shared_mutex> workspaceCreateShareLock(workspaceCreateSmtx);
  unsigned int currentCreateWorkspaceNum = 0;
  for (Json::Value const &workspaceToCreate : m_workspacesToCreate) {
    createWorkspace(workspaceToCreate);
    currentCreateWorkspaceNum++;
  }
  for (unsigned int i = 0; i < currentCreateWorkspaceNum; i++) {
    m_workspacesToCreate.erase(m_workspacesToCreate.begin());
  }

  // get all active workspaces
  auto monitors = gIPC->getSocket1JsonReply("monitors");
  std::vector<std::string> visibleWorkspaces;
  for (Json::Value &monitor : monitors) {
    auto ws = monitor["activeWorkspace"];
    if (ws.isObject() && (ws["name"].isString())) {
      visibleWorkspaces.push_back(ws["name"].asString());
    }
  }

  for (auto &workspace : m_workspaces) {
    // active
    workspace->setActive(workspace->name() == m_activeWorkspaceName);
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
      workspaceIcon = workspace->selectIcon(m_iconsMap);
    }
    workspace->update(m_format, workspaceIcon);
  }

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
      }
    }
  }

  if (anyWindowCreated) {
    dp.emit();
  }

  m_windowsToCreate.clear();
  m_windowsToCreate = notCreated;

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
  } else if (eventName == "destroyworkspace") {
    onWorkspaceDestroyed(payload);
  } else if (eventName == "createworkspace") {
    onWorkspaceCreated(payload);
  } else if (eventName == "focusedmon") {
    onMonitorFocused(payload);
  } else if (eventName == "moveworkspace" && !allOutputs()) {
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
  }

  dp.emit();
}

void Workspaces::onWorkspaceActivated(std::string const &payload) {
  m_activeWorkspaceName = payload;
}

void Workspaces::onWorkspaceDestroyed(std::string const &payload) {
  if (!isDoubleSpecial(payload)) {
    m_workspacesToRemove.push_back(payload);
  }
}

void Workspaces::onWorkspaceCreated(std::string const &payload) {
  const Json::Value workspacesJson = gIPC->getSocket1JsonReply("workspaces");

  if (!isWorkspaceIgnored(payload)) {
    for (Json::Value workspaceJson : workspacesJson) {
      std::string name = workspaceJson["name"].asString();
      if (name == payload &&
          (allOutputs() || m_bar.output->name == workspaceJson["monitor"].asString()) &&
          (showSpecial() || !name.starts_with("special")) && !isDoubleSpecial(payload)) {
        std::unique_lock<std::shared_mutex> workspaceCreateUniqueLock(workspaceCreateSmtx);
        m_workspacesToCreate.push_back(workspaceJson);
        break;
      }
    }
  }
}

void Workspaces::onWorkspaceMoved(std::string const &payload) {
  std::string workspace = payload.substr(0, payload.find(','));
  std::string newOutput = payload.substr(payload.find(',') + 1);
  bool shouldShow = showSpecial() || !workspace.starts_with("special");
  if (shouldShow && m_bar.output->name == newOutput) {  // TODO: implement this better
    const Json::Value workspacesJson = gIPC->getSocket1JsonReply("workspaces");
    for (Json::Value workspaceJson : workspacesJson) {
      std::string name = workspaceJson["name"].asString();
      if (name == workspace && m_bar.output->name == workspaceJson["monitor"].asString()) {
        m_workspacesToCreate.push_back(workspaceJson);
        break;
      }
    }
  } else {
    m_workspacesToRemove.push_back(workspace);
  }
}

void Workspaces::onWorkspaceRenamed(std::string const &payload) {
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
  m_activeWorkspaceName = payload.substr(payload.find(',') + 1);
}

void Workspaces::onWindowOpened(std::string const &payload) {
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
  updateWindowCount();
  for (auto &workspace : m_workspaces) {
    if (workspace->closeWindow(addr)) {
      break;
    }
  }
}

void Workspaces::onWindowMoved(std::string const &payload) {
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

  // ...and add it to the new workspace
  if (!windowRepr.empty()) {
    m_windowsToCreate.emplace_back(workspaceName, windowAddress, windowRepr);
  }
}

void Workspaces::onWindowTitleEvent(std::string const &payload) {
  auto windowWorkspace =
      std::find_if(m_workspaces.begin(), m_workspaces.end(),
                   [payload](auto &workspace) { return workspace->containsWindow(payload); });

  if (windowWorkspace != m_workspaces.end()) {
    Json::Value clientsData = gIPC->getSocket1JsonReply("clients");
    std::string jsonWindowAddress = fmt::format("0x{}", payload);

    auto client =
        std::find_if(clientsData.begin(), clientsData.end(), [jsonWindowAddress](auto &client) {
          return client["address"].asString() == jsonWindowAddress;
        });

    if (!client->empty()) {
      (*windowWorkspace)->insertWindow({*client});
    }
  }
}

void Workspaces::updateWindowCount() {
  const Json::Value workspacesJson = gIPC->getSocket1JsonReply("workspaces");
  for (auto &workspace : m_workspaces) {
    auto workspaceJson = std::find_if(
        workspacesJson.begin(), workspacesJson.end(),
        [&](Json::Value const &x) { return x["name"].asString() == workspace->name(); });
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
  // avoid recreating existing workspaces
  auto workspaceName = workspace_data["name"].asString();
  auto workspace = std::find_if(
      m_workspaces.begin(), m_workspaces.end(),
      [workspaceName](std::unique_ptr<Workspace> const &w) {
        return (workspaceName.starts_with("special:") && workspaceName.substr(8) == w->name()) ||
               workspaceName == w->name();
      });

  if (workspace != m_workspaces.end()) {
    if (workspace_data["persistent"].asBool() and !(*workspace)->isPersistent()) {
      (*workspace)->setPersistent();
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
  auto workspace =
      std::find_if(m_workspaces.begin(), m_workspaces.end(), [&](std::unique_ptr<Workspace> &x) {
        return (name.starts_with("special:") && name.substr(8) == x->name()) || name == x->name();
      });

  if (workspace == m_workspaces.end()) {
    // happens when a workspace on another monitor is destroyed
    return;
  }

  if ((*workspace)->isPersistent()) {
    // don't remove persistent workspaces, createWorkspace will take care of replacement
    return;
  }

  m_box.remove(workspace->get()->button());
  m_workspaces.erase(workspace);
}

void Workspaces::fillPersistentWorkspaces() {
  if (config_["persistent_workspaces"].isObject()) {
    spdlog::warn(
        "persistent_workspaces is deprecated. Please change config to use persistent-workspaces.");
  }

  if (config_["persistent-workspaces"].isObject() || config_["persistent_workspaces"].isObject()) {
    const Json::Value persistentWorkspaces = config_["persistent-workspaces"].isObject()
                                                 ? config_["persistent-workspaces"]
                                                 : config_["persistent_workspaces"];
    const std::vector<std::string> keys = persistentWorkspaces.getMemberNames();

    for (const std::string &key : keys) {
      // only add if either:
      // 1. key is "*" and this monitor is not already defined in the config
      // 2. key is the current monitor name
      bool canCreate =
          (key == "*" && std::find(keys.begin(), keys.end(), m_bar.output->name) == keys.end()) ||
          key == m_bar.output->name;
      const Json::Value &value = persistentWorkspaces[key];

      if (value.isInt()) {
        // value is a number => create that many workspaces for this monitor
        if (canCreate) {
          int amount = value.asInt();
          spdlog::debug("Creating {} persistent workspaces for monitor {}", amount,
                        m_bar.output->name);
          for (int i = 0; i < amount; i++) {
            m_persistentWorkspacesToCreate.emplace_back(
                std::to_string(m_monitorId * amount + i + 1));
          }
        }
      } else if (value.isArray() && !value.empty()) {
        // value is an array => create defined workspaces for this monitor
        if (canCreate) {
          for (const Json::Value &workspace : value) {
            if (workspace.isInt()) {
              spdlog::debug("Creating workspace {} on monitor {}", workspace, m_bar.output->name);
              m_persistentWorkspacesToCreate.emplace_back(std::to_string(workspace.asInt()));
            }
          }
        } else {
          // key is the workspace and value is array of monitors to create on
          for (const Json::Value &monitor : value) {
            if (monitor.isString() && monitor.asString() == m_bar.output->name) {
              m_persistentWorkspacesToCreate.emplace_back(key);
              break;
            }
          }
        }
      } else {
        // this workspace should be displayed on all monitors
        m_persistentWorkspacesToCreate.emplace_back(key);
      }
    }
  }
}

void Workspaces::createPersistentWorkspaces() {
  for (const std::string &workspaceName : m_persistentWorkspacesToCreate) {
    Json::Value newWorkspace;
    try {
      // numbered persistent workspaces get the name as ID
      newWorkspace["id"] = workspaceName == "special" ? -99 : std::stoi(workspaceName);
    } catch (const std::exception &e) {
      // named persistent workspaces start with ID=0
      newWorkspace["id"] = 0;
    }
    newWorkspace["name"] = workspaceName;
    newWorkspace["monitor"] = m_bar.output->name;
    newWorkspace["windows"] = 0;
    newWorkspace["persistent"] = true;

    createWorkspace(newWorkspace);
  }
}

void Workspaces::init() {
  m_activeWorkspaceName = (gIPC->getSocket1JsonReply("activeworkspace"))["name"].asString();

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
  }

  const Json::Value workspacesJson = gIPC->getSocket1JsonReply("workspaces");
  const Json::Value clientsJson = gIPC->getSocket1JsonReply("clients");

  for (Json::Value workspaceJson : workspacesJson) {
    std::string workspaceName = workspaceJson["name"].asString();
    if ((allOutputs() || m_bar.output->name == workspaceJson["monitor"].asString()) &&
        (!workspaceName.starts_with("special") || showSpecial()) &&
        !isWorkspaceIgnored(workspaceName)) {
      createWorkspace(workspaceJson, clientsJson);
    }
  }

  fillPersistentWorkspaces();
  createPersistentWorkspaces();

  updateWindowCount();

  sortWorkspaces();

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
      m_active(true) {
  if (m_name.starts_with("name:")) {
    m_name = m_name.substr(5);
  } else if (m_name.starts_with("special")) {
    m_name = m_id == -99 ? m_name : m_name.substr(8);
    m_isSpecial = true;
  }

  if (workspace_data.isMember("persistent")) {
    m_isPersistent = workspace_data["persistent"].asBool();
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

void Workspace::update(const std::string &format, const std::string &icon) {
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
                    // both are 0 (not yet named persistents) / both are named specials (-98 <= ID
                    // <=-1)
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

std::string &Workspace::selectIcon(std::map<std::string, std::string> &icons_map) {
  if (isUrgent()) {
    auto urgentIconIt = icons_map.find("urgent");
    if (urgentIconIt != icons_map.end()) {
      return urgentIconIt->second;
    }
  }

  if (isActive()) {
    auto activeIconIt = icons_map.find("active");
    if (activeIconIt != icons_map.end()) {
      return activeIconIt->second;
    }
  }

  if (isSpecial()) {
    auto specialIconIt = icons_map.find("special");
    if (specialIconIt != icons_map.end()) {
      return specialIconIt->second;
    }
  }

  auto namedIconIt = icons_map.find(name());
  if (namedIconIt != icons_map.end()) {
    return namedIconIt->second;
  }

  if (isVisible()) {
    auto visibleIconIt = icons_map.find("visible");
    if (visibleIconIt != icons_map.end()) {
      return visibleIconIt->second;
    }
  }

  if (isEmpty()) {
    auto emptyIconIt = icons_map.find("empty");
    if (emptyIconIt != icons_map.end()) {
      return emptyIconIt->second;
    }
  }

  if (isPersistent()) {
    auto persistentIconIt = icons_map.find("persistent");
    if (persistentIconIt != icons_map.end()) {
      return persistentIconIt->second;
    }
  }

  auto defaultIconIt = icons_map.find("default");
  if (defaultIconIt != icons_map.end()) {
    return defaultIconIt->second;
  }

  return m_name;
}

bool Workspace::handleClicked(GdkEventButton *bt) const {
  if (bt->type == GDK_BUTTON_PRESS) {
    try {
      if (id() > 0) {  // normal or numbered persistent
        gIPC->getSocket1Reply("dispatch workspace " + std::to_string(id()));
      } else if (!isSpecial()) {  // named
        gIPC->getSocket1Reply("dispatch workspace name:" + name());
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
  return m_windowRewriteRules.get(windowReprKey);
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
}

void WindowCreationPayload::moveToWorksace(std::string &new_workspace_name) {
  m_workspaceName = new_workspace_name;
}

}  // namespace waybar::modules::hyprland
