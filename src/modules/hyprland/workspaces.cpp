#include "modules/hyprland/workspaces.hpp"

#include <json/value.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "util/gtk/gtk_icon.hpp"
#include "util/regex_collection.hpp"
#include "util/string.hpp"

namespace waybar::modules::hyprland {

Workspaces::Workspaces(const std::string& id, const Bar& bar, const Json::Value& config)
    : AModule(config, "workspaces", id, false, false),
      m_bar_(bar),
      m_box_(bar.orientation, 0),
      m_ipc_(IPC::inst()) {
  w_ = &m_box_;
  parseConfig(config);

  m_box_.set_name("workspaces");
  if (!id.empty()) {
    m_box_.get_style_context()->add_class(id);
  }
  m_box_.get_style_context()->add_class(MODULE_CLASS);

  setCurrentMonitorId();
  init();
  registerIpc();
}

Workspaces::~Workspaces() {
  if (m_scrollEventConnection_.connected()) {
    m_scrollEventConnection_.disconnect();
  }
  // Cancel any pending debounce timeout so it cannot fire on a freed `this`.
  // Runs on the main thread, same as where the timer is armed.
  if (m_debounceTimer_.connected()) {
    m_debounceTimer_.disconnect();
  }
  m_ipc_.unregisterForIPC(this);
  // wait for possible event handler to finish
  std::lock_guard<std::mutex> lg(m_mutex_);
}

void Workspaces::init() {
  m_activeWorkspaceId_ = m_ipc_.getSocket1JsonReply("activeworkspace")["id"].asInt();

  initializeWorkspaces();

  if (m_scrollEventConnection_.connected()) {
    m_scrollEventConnection_.disconnect();
  }

  bindEvents(m_box_);
  controller_scroll_->set_propagation_phase(Gtk::PropagationPhase::BUBBLE);

  dp.emit();
}

Json::Value Workspaces::createMonitorWorkspaceData(std::string const& name,
                                                   std::string const& monitor) {
  spdlog::trace("Creating persistent workspace: {} on monitor {}", name, monitor);
  Json::Value workspaceData;

  auto workspaceId = parseWorkspaceId(name);
  if (!workspaceId.has_value()) {
    workspaceId = 0;
  }
  workspaceData["id"] = *workspaceId;
  workspaceData["name"] = name;
  workspaceData["monitor"] = monitor;
  workspaceData["windows"] = 0;
  return workspaceData;
}

void Workspaces::createWorkspace(Json::Value const& workspace_data,
                                 Json::Value const& clients_data) {
  auto workspaceId = workspace_data["id"].asInt();
  spdlog::debug("Creating workspace {}", workspaceId);

  // avoid recreating existing workspaces
  auto workspace = std::ranges::find_if(
      m_workspaces_,
      [workspaceId](std::unique_ptr<Workspace> const& w) { return workspaceId == w->id(); });

  if (workspace != m_workspaces_.end()) {
    // don't recreate workspace, but update persistency if necessary
    const auto keys = workspace_data.getMemberNames();

    const auto* k = "persistent-rule";
    if (std::ranges::find(keys, k) != keys.end()) {
      spdlog::debug("Set dynamic persistency of workspace {} to: {}", workspaceId,
                    workspace_data[k].asBool() ? "true" : "false");
      (*workspace)->setPersistentRule(workspace_data[k].asBool());
    }

    k = "persistent-config";
    if (std::ranges::find(keys, k) != keys.end()) {
      spdlog::debug("Set config persistency of workspace {} to: {}", workspaceId,
                    workspace_data[k].asBool() ? "true" : "false");
      (*workspace)->setPersistentConfig(workspace_data[k].asBool());
    }

    return;
  }

  // create new workspace
  m_workspaces_.emplace_back(std::make_unique<Workspace>(workspace_data, *this, clients_data));
  Gtk::Button& newWorkspaceButton = m_workspaces_.back()->button();
  newWorkspaceButton.set_expand(false);
  m_box_.append(newWorkspaceButton);
  sortWorkspaces();
  newWorkspaceButton.show();
}

void Workspaces::createWorkspacesToCreate() {
  for (const auto& [workspaceData, clientsData] : m_workspacesToCreate_) {
    createWorkspace(workspaceData, clientsData);
  }
  if (!m_workspacesToCreate_.empty()) {
    updateWindowCount();
    sortWorkspaces();
  }
  m_workspacesToCreate_.clear();
}

void Workspaces::extendOrphans(int workspaceId, Json::Value const& clientsJson) {
  spdlog::trace("Extending orphans with workspace {}", workspaceId);
  for (const auto& client : clientsJson) {
    if (client["workspace"]["id"].asInt() == workspaceId) {
      registerOrphanWindow({client});
    }
  }
}

std::string Workspaces::getRewrite(const std::string& window_class,
                                   const std::string& window_title) {
  std::string windowReprKey;
  if (windowRewriteConfigUsesTitle()) {
    windowReprKey = fmt::format("class<{}> title<{}>", window_class, window_title);
  } else {
    windowReprKey = fmt::format("class<{}>", window_class);
  }
  auto const rewriteRule = m_windowRewriteRules_.get(windowReprKey);
  return fmt::format(fmt::runtime(rewriteRule), fmt::arg("class", window_class),
                     fmt::arg("title", window_title));
}

std::vector<int> Workspaces::getVisibleWorkspaces() {
  std::vector<int> visibleWorkspaces;
  auto monitors = IPC::inst().getSocket1JsonReply("monitors");
  for (const auto& monitor : monitors) {
    auto ws = monitor["activeWorkspace"];
    if (ws.isObject() && ws["id"].isInt()) {
      visibleWorkspaces.push_back(ws["id"].asInt());
    }
    auto sws = monitor["specialWorkspace"];
    auto name = sws["name"].asString();
    if (sws.isObject() && sws["id"].isInt() && !name.empty()) {
      visibleWorkspaces.push_back(sws["id"].asInt());
    }
  }
  return visibleWorkspaces;
}

void Workspaces::initializeWorkspaces() {
  spdlog::debug("Initializing workspaces");

  // if the workspace rules changed since last initialization, make sure we reset everything:
  for (auto& workspace : m_workspaces_) {
    m_workspacesToRemove_.push_back(std::to_string(workspace->id()));
  }

  // get all current workspaces
  auto const workspacesJson = m_ipc_.getSocket1JsonReply("workspaces");
  auto const clientsJson = m_ipc_.getSocket1JsonReply("clients");

  for (const auto& workspaceJson : workspacesJson) {
    std::string workspaceName = workspaceJson["name"].asString();
    if ((allOutputs() || m_bar_.output->name == workspaceJson["monitor"].asString()) &&
        (!workspaceName.starts_with("special") || showSpecial()) &&
        !isWorkspaceIgnored(workspaceName)) {
      m_workspacesToCreate_.emplace_back(workspaceJson, clientsJson);
    } else {
      extendOrphans(workspaceJson["id"].asInt(), clientsJson);
    }
  }

  spdlog::debug("Initializing persistent workspaces");
  if (m_persistentWorkspaceConfig_.isObject()) {
    // a persistent workspace config is defined, so use that instead of workspace rules
    loadPersistentWorkspacesFromConfig(clientsJson);
  }
}

bool isDoubleSpecial(std::string const& workspace_name) {
  // Hyprland's IPC sometimes reports the creation of workspaces strangely named
  // `special:special:<some_name>`. This function checks for that and is used
  // to avoid creating (and then removing) such workspaces.
  // See hyprwm/Hyprland#3424 for more info.
  return workspace_name.find("special:special:") != std::string::npos;
}

bool Workspaces::isWorkspaceIgnored(std::string const& name) {
  for (auto& rule : m_ignoreWorkspaces_) {
    if (std::regex_match(name, rule)) {
      return true;
      break;
    }
  }

  return false;
}

void Workspaces::loadPersistentWorkspacesFromConfig(Json::Value const& clientsJson) {
  spdlog::info("Loading persistent workspaces from Waybar config");
  const std::vector<std::string> keys = m_persistentWorkspaceConfig_.getMemberNames();
  std::vector<std::string> persistentWorkspacesToCreate;

  const std::string currentMonitor = m_bar_.output->name;
  const bool monitorInConfig = std::ranges::find(keys, currentMonitor) != keys.end();
  for (const std::string& key : keys) {
    // only add if either:
    // 1. key is the current monitor name
    // 2. key is "*" and this monitor is not already defined in the config
    bool canCreate = key == currentMonitor || (key == "*" && !monitorInConfig);
    const Json::Value& value = m_persistentWorkspaceConfig_[key];
    spdlog::trace("Parsing persistent workspace config: {} => {}", key, value.toStyledString());

    if (value.isInt()) {
      // value is a number => create that many workspaces for this monitor
      if (canCreate) {
        int amount = value.asInt();
        spdlog::debug("Creating {} persistent workspaces for monitor {}", amount, currentMonitor);
        for (int i = 0; i < amount; i++) {
          persistentWorkspacesToCreate.emplace_back(
              std::to_string((m_monitorId_ * amount) + i + 1));
        }
      }
    } else if (value.isArray() && !value.empty()) {
      // value is an array => create defined workspaces for this monitor
      if (canCreate) {
        for (const Json::Value& workspace : value) {
          spdlog::debug("Creating workspace {} on monitor {}", workspace, currentMonitor);
          persistentWorkspacesToCreate.emplace_back(workspace.asString());
        }
      } else {
        // key is the workspace and value is array of monitors to create on
        for (const Json::Value& monitor : value) {
          if (monitor.isString() && monitor.asString() == currentMonitor) {
            persistentWorkspacesToCreate.emplace_back(key);
            break;
          }
        }
      }
    } else {
      // this workspace should be displayed on all monitors
      persistentWorkspacesToCreate.emplace_back(key);
    }
  }

  for (auto const& workspace : persistentWorkspacesToCreate) {
    auto workspaceData = createMonitorWorkspaceData(workspace, m_bar_.output->name);
    workspaceData["persistent-config"] = true;
    m_workspacesToCreate_.emplace_back(workspaceData, clientsJson);
  }
}

void Workspaces::onEvent(const std::string& ev) {
  {
    std::lock_guard<std::mutex> lock(m_mutex_);
    const auto separator = ev.find(">>");
    if (separator == std::string::npos) {
      spdlog::warn("Malformed Hyprland workspace event: {}", ev);
      return;
    }
    std::string eventName = ev.substr(0, separator);
    std::string payload = ev.substr(separator + 2);

    if (eventName == "workspacev2") {
      onWorkspaceActivated(payload);
    } else if (eventName == "activespecial") {
      onSpecialWorkspaceActivated(payload);
    } else if (eventName == "destroyworkspacev2") {
      onWorkspaceDestroyed(payload);
    } else if (eventName == "createworkspacev2") {
      onWorkspaceCreated(payload);
    } else if (eventName == "focusedmonv2") {
      onMonitorFocused(payload);
    } else if (eventName == "moveworkspacev2") {
      onWorkspaceMoved(payload);
    } else if (eventName == "openwindow") {
      onWindowOpened(payload);
    } else if (eventName == "closewindow") {
      onWindowClosed(payload);
    } else if (eventName == "movewindowv2") {
      onWindowMoved(payload);
    } else if (eventName == "urgent") {
      setUrgentWorkspace(payload);
    } else if (eventName == "renameworkspace") {
      onWorkspaceRenamed(payload);
    } else if (eventName == "windowtitlev2") {
      onWindowTitleEvent(payload);
    } else if (eventName == "activewindowv2") {
      onActiveWindowChanged(payload);
    } else if (eventName == "configreloaded") {
      onConfigReloaded();
    }
  }

  // Notify the main thread. dp (Glib::Dispatcher) is the only thread-safe way to
  // hand off to the GTK main loop; GLib timer state must never be touched from the
  // IPC listener thread. The debounce timer is owned entirely by the main-thread
  // update() path (see Workspaces::update).
  dp.emit();
}

void Workspaces::onWorkspaceActivated(std::string const& payload) {
  const auto [workspaceIdStr, workspaceName] = splitDoublePayload(payload);
  const auto workspaceId = parseWorkspaceId(workspaceIdStr);
  if (workspaceId.has_value()) {
    m_activeWorkspaceId_ = *workspaceId;
  }
}

void Workspaces::onSpecialWorkspaceActivated(std::string const& payload) {
  std::string name(begin(payload), begin(payload) + payload.find_first_of(','));
  m_activeSpecialWorkspaceName_ = (!name.starts_with("special:") ? name : name.substr(8));
}

void Workspaces::onWorkspaceDestroyed(std::string const& payload) {
  const auto [workspaceId, workspaceName] = splitDoublePayload(payload);
  if (!isDoubleSpecial(workspaceName)) {
    m_workspacesToRemove_.push_back(workspaceId);
  }
}

void Workspaces::onWorkspaceCreated(std::string const& payload, Json::Value const& clientsData) {
  spdlog::debug("Workspace created: {}", payload);

  const auto [workspaceIdStr, _] = splitDoublePayload(payload);

  const auto workspaceId = parseWorkspaceId(workspaceIdStr);
  if (!workspaceId.has_value()) {
    return;
  }

  auto const workspaceRules = m_ipc_.getSocket1JsonReply("workspacerules");
  auto const workspacesJson = m_ipc_.getSocket1JsonReply("workspaces");

  for (auto workspaceJson : workspacesJson) {
    const auto currentId = workspaceJson["id"].asInt();
    if (currentId == *workspaceId) {
      std::string workspaceName = workspaceJson["name"].asString();
      // This workspace name is more up-to-date than the one in the event payload.
      if (isWorkspaceIgnored(workspaceName)) {
        spdlog::trace("Not creating workspace because it is ignored: id={} name={}", *workspaceId,
                      workspaceName);
        break;
      }

      if ((allOutputs() || m_bar_.output->name == workspaceJson["monitor"].asString()) &&
          (showSpecial() || !workspaceName.starts_with("special")) &&
          !isDoubleSpecial(workspaceName)) {
        for (Json::Value const& rule : workspaceRules) {
          auto ruleWorkspaceName = rule.isMember("defaultName")
                                       ? rule["defaultName"].asString()
                                       : rule["workspaceString"].asString();
          if (ruleWorkspaceName == workspaceName) {
            workspaceJson["persistent-rule"] = rule["persistent"].asBool();
            break;
          }
        }

        m_workspacesToCreate_.emplace_back(workspaceJson, clientsData);
        break;
      }
    } else {
      extendOrphans(*workspaceId, clientsData);
    }
  }
}

void Workspaces::onWorkspaceMoved(std::string const& payload) {
  spdlog::debug("Workspace moved: {}", payload);

  // Update active workspace
  m_activeWorkspaceId_ = (m_ipc_.getSocket1JsonReply("activeworkspace"))["id"].asInt();

  if (allOutputs()) return;

  const auto [workspaceIdStr, workspaceName, monitorName] = splitTriplePayload(payload);

  const auto subPayload = makePayload(workspaceIdStr, workspaceName);

  if (m_bar_.output->name == monitorName) {
    Json::Value clientsData = m_ipc_.getSocket1JsonReply("clients");
    onWorkspaceCreated(subPayload, clientsData);
  } else {
    spdlog::debug("Removing workspace because it was moved to another monitor: {}", subPayload);
    onWorkspaceDestroyed(subPayload);
  }
}

void Workspaces::onWorkspaceRenamed(std::string const& payload) {
  spdlog::debug("Workspace renamed: {}", payload);
  const auto [workspaceIdStr, newName] = splitDoublePayload(payload);

  const auto workspaceId = parseWorkspaceId(workspaceIdStr);
  if (!workspaceId.has_value()) {
    return;
  }

  for (auto& workspace : m_workspaces_) {
    if (workspace->id() == *workspaceId) {
      workspace->setName(newName);
      break;
    }
  }
  sortWorkspaces();
}

void Workspaces::onMonitorFocused(std::string const& payload) {
  spdlog::trace("Monitor focused: {}", payload);

  const auto [monitorName, workspaceIdStr] = splitDoublePayload(payload);

  const auto workspaceId = parseWorkspaceId(workspaceIdStr);
  if (!workspaceId.has_value()) {
    return;
  }

  m_activeWorkspaceId_ = *workspaceId;

  for (Json::Value& monitor : m_ipc_.getSocket1JsonReply("monitors")) {
    if (monitor["name"].asString() == monitorName) {
      const auto name = monitor["specialWorkspace"]["name"].asString();
      m_activeSpecialWorkspaceName_ = !name.starts_with("special:") ? name : name.substr(8);
    }
  }
}

void Workspaces::onWindowOpened(std::string const& payload) {
  spdlog::trace("Window opened: {}", payload);
  updateWindowCount();
  const auto firstComma = payload.find(',');
  const auto secondComma =
      firstComma == std::string::npos ? std::string::npos : payload.find(',', firstComma + 1);
  const auto thirdComma =
      secondComma == std::string::npos ? std::string::npos : payload.find(',', secondComma + 1);
  if (firstComma == std::string::npos || secondComma == std::string::npos ||
      thirdComma == std::string::npos) {
    spdlog::warn("Malformed Hyprland openwindow payload: {}", payload);
    return;
  }

  std::string windowAddress = payload.substr(0, firstComma);
  std::string workspaceName = payload.substr(firstComma + 1, secondComma - firstComma - 1);
  std::string windowClass = payload.substr(secondComma + 1, thirdComma - secondComma - 1);
  std::string windowTitle = payload.substr(thirdComma + 1);

  bool isActive = m_currentActiveWindowAddress_ == windowAddress;
  m_windowsToCreate_.emplace_back(workspaceName, windowAddress, windowClass, windowTitle, isActive);
}

void Workspaces::onWindowClosed(std::string const& addr) {
  spdlog::trace("Window closed: {}", addr);
  updateWindowCount();
  m_orphanWindowMap_.erase(addr);
  for (auto& workspace : m_workspaces_) {
    if (workspace->closeWindow(addr)) {
      break;
    }
  }
}

void Workspaces::onWindowMoved(std::string const& payload) {
  spdlog::trace("Window moved: {}", payload);
  updateWindowCount();
  auto [windowAddress, _, workspaceName] = splitTriplePayload(payload);

  WindowRepr windowRepr;

  // If the window was still queued to be created, just change its destination
  // and exit
  for (auto& window : m_windowsToCreate_) {
    if (window.getAddress() == windowAddress) {
      window.moveToWorkspace(workspaceName);
      return;
    }
  }

  // Take the window's representation from the old workspace...
  for (auto& workspace : m_workspaces_) {
    if (auto windowAddr = workspace->closeWindow(windowAddress); windowAddr != std::nullopt) {
      windowRepr = windowAddr.value();
      break;
    }
  }

  // ...if it was empty, check if the window is an orphan...
  if (windowRepr.empty() && m_orphanWindowMap_.contains(windowAddress)) {
    windowRepr = m_orphanWindowMap_[windowAddress];
  }

  // ...and then add it to the new workspace
  if (!windowRepr.empty()) {
    m_orphanWindowMap_.erase(windowAddress);
    m_windowsToCreate_.emplace_back(workspaceName, windowAddress, windowRepr);
  }
}

void Workspaces::onWindowTitleEvent(std::string const& payload) {
  spdlog::trace("Window title changed: {}", payload);
  std::optional<std::function<void(WindowCreationPayload)>> inserter;

  const auto [windowAddress, _] = splitDoublePayload(payload);

  // If the window was an orphan, rename it at the orphan's vector
  if (m_orphanWindowMap_.contains(windowAddress)) {
    inserter = [this](WindowCreationPayload wcp) { this->registerOrphanWindow(std::move(wcp)); };
  } else {
    auto windowWorkspace = std::ranges::find_if(m_workspaces_, [windowAddress](auto& workspace) {
      return workspace->containsWindow(windowAddress);
    });

    // If the window exists on a workspace, rename it at the workspace's window
    // map
    if (windowWorkspace != m_workspaces_.end()) {
      inserter = [windowWorkspace](WindowCreationPayload wcp) {
        (*windowWorkspace)->insertWindow(std::move(wcp));
      };
    } else {
      auto queuedWindow =
          std::ranges::find_if(m_windowsToCreate_, [&windowAddress](auto& windowPayload) {
            return windowPayload.getAddress() == windowAddress;
          });

      // If the window was queued, rename it in the queue
      if (queuedWindow != m_windowsToCreate_.end()) {
        inserter = [queuedWindow](WindowCreationPayload wcp) { *queuedWindow = std::move(wcp); };
      }
    }
  }

  if (inserter.has_value()) {
    Json::Value clientsData = m_ipc_.getSocket1JsonReply("clients");
    std::string jsonWindowAddress = fmt::format("0x{}", windowAddress);

    auto client = std::ranges::find_if(clientsData, [jsonWindowAddress](auto& client) {
      return client["address"].asString() == jsonWindowAddress;
    });

    if (client != clientsData.end() && !client->empty()) {
      (*inserter)({*client});
    }
  }
}

void Workspaces::onActiveWindowChanged(WindowAddress const& activeWindowAddress) {
  spdlog::trace("Active window changed: {}", activeWindowAddress);
  m_currentActiveWindowAddress_ = activeWindowAddress;

  for (auto& [address, window] : m_orphanWindowMap_) {
    window.setActive(address == activeWindowAddress);
  }
  for (auto const& workspace : m_workspaces_) {
    workspace->setActiveWindow(activeWindowAddress);
  }
  for (auto& window : m_windowsToCreate_) {
    window.setActive(window.getAddress() == activeWindowAddress);
  }
}

void Workspaces::onConfigReloaded() {
  spdlog::info("Hyprland config reloaded, reinitializing hyprland/workspaces module...");
  init();
}

auto Workspaces::parseConfig(const Json::Value& config) -> void {
  const auto& configFormat = config["format"];
  m_formatBefore_ = configFormat.isString() ? configFormat.asString() : "{name}";
  m_withIcon_ = m_formatBefore_.find("{icon}") != std::string::npos;
  auto withWindows = m_formatBefore_.find("{windows}") != std::string::npos;

  if (m_withIcon_ && m_iconsMap_.empty()) {
    populateIconsMap(config["format-icons"]);
  }

  m_withTooltip_ = tooltipEnabled();
  if (m_withTooltip_ && m_tooltipMap_.empty()) {
    const Json::Value& tooltipFormats = config["tooltips"];
    for (const auto& name : tooltipFormats.getMemberNames()) {
      m_tooltipMap_.emplace(name, tooltipFormats[name].asString());
    }
    m_tooltipMap_.emplace("", "");
  }

  populateBoolConfig(config, "all-outputs", m_allOutputs_);
  populateBoolConfig(config, "show-special", m_showSpecial_);
  populateBoolConfig(config, "special-visible-only", m_specialVisibleOnly_);
  populateBoolConfig(config, "persistent-only", m_persistentOnly_);
  populateBoolConfig(config, "active-only", m_activeOnly_);
  populateBoolConfig(config, "hide-active", m_hideActive_);
  populateBoolConfig(config, "move-to-monitor", m_moveToMonitor_);
  populateBoolConfig(config, "unique-icons", m_uniqueIcons_);
  populateBoolConfig(config, "enable-bar-scroll", m_barScroll_);

  m_persistentWorkspaceConfig_ = config.get("persistent-workspaces", Json::Value());
  populateSortByConfig(config);
  populateIgnoreWorkspacesConfig(config);
  populateFormatWindowSeparatorConfig(config);

  const auto& groupThreshold = config["window-rewrite-group-threshold"];
  if (groupThreshold.isInt()) {
    m_windowRewriteGroupThreshold_ = groupThreshold.asInt();
  }
  const auto& groupFormat = config["window-rewrite-group-format"];
  if (groupFormat.isString()) {
    m_windowRewriteGroupFormat_ = groupFormat.asString();
  }

  populateWindowRewriteConfig(config);
  populateMaxWindowsConfig(config);

  if (withWindows) {
    populateWorkspaceTaskbarConfig(config);
  }
  if (m_enableTaskbar_) {
    auto parts = split(m_formatBefore_, "{windows}", 1);
    m_formatBefore_ = parts[0];
    m_formatAfter_ = parts.size() > 1 ? parts[1] : "";
  }
}

auto Workspaces::populateIconsMap(const Json::Value& formatIcons) -> void {
  for (const auto& name : formatIcons.getMemberNames()) {
    m_iconsMap_.emplace(name, formatIcons[name].asString());
  }
  m_iconsMap_.emplace("", "");
}

auto Workspaces::populateBoolConfig(const Json::Value& config, const std::string& key, bool& member)
    -> void {
  const auto& configValue = config[key];
  if (configValue.isBool()) {
    member = configValue.asBool();
  }
}

auto Workspaces::populateSortByConfig(const Json::Value& config) -> void {
  const auto& configSortBy = config["sort-by"];
  if (configSortBy.isString()) {
    auto sortByStr = configSortBy.asString();
    try {
      m_sortBy_ = m_enumParser_.parseStringToEnum(sortByStr, m_sortMap_);
    } catch (const std::invalid_argument& e) {
      m_sortBy_ = SortMethod::DEFAULT;
      spdlog::warn(
          "Invalid string representation for sort-by. Falling back to default sort method.");
    }
  }
}

auto Workspaces::populateIgnoreWorkspacesConfig(const Json::Value& config) -> void {
  auto ignoreWorkspaces = config["ignore-workspaces"];
  if (ignoreWorkspaces.isArray()) {
    for (const auto& workspaceRegex : ignoreWorkspaces) {
      if (workspaceRegex.isString()) {
        std::string ruleString = workspaceRegex.asString();
        try {
          const std::regex rule{ruleString, std::regex_constants::icase};
          m_ignoreWorkspaces_.emplace_back(rule);
        } catch (const std::regex_error& e) {
          spdlog::error("Invalid rule {}: {}", ruleString, e.what());
        }
      } else {
        spdlog::error("Not a string: '{}'", workspaceRegex);
      }
    }
  }
}

auto Workspaces::populateFormatWindowSeparatorConfig(const Json::Value& config) -> void {
  const auto& formatWindowSeparator = config["format-window-separator"];
  m_formatWindowSeparator_ =
      formatWindowSeparator.isString() ? formatWindowSeparator.asString() : " ";
}

auto Workspaces::populateWindowRewriteConfig(const Json::Value& config) -> void {
  const auto& windowRewrite = config["window-rewrite"];
  if (!windowRewrite.isObject()) {
    spdlog::debug("window-rewrite is not defined or is not an object, using default rules.");
    return;
  }

  const auto& windowRewriteDefaultConfig = config["window-rewrite-default"];
  std::string windowRewriteDefault =
      windowRewriteDefaultConfig.isString() ? windowRewriteDefaultConfig.asString() : "?";

  m_windowRewriteRules_ = util::RegexCollection(
      windowRewrite, windowRewriteDefault,
      [this](std::string& window_rule) { return windowRewritePriorityFunction(window_rule); });
}

auto Workspaces::populateMaxWindowsConfig(const Json::Value& config) -> void {
  if (config["max-windows"].isInt()) {
    m_maxWindows_ = config["max-windows"].asInt();
    if (m_maxWindows_ < 0) {
      m_maxWindows_ = 0;
    }
  }
}

auto Workspaces::populateWorkspaceTaskbarConfig(const Json::Value& config) -> void {
  const auto& workspaceTaskbar = config["workspace-taskbar"];
  if (!workspaceTaskbar.isObject()) {
    spdlog::debug("workspace-taskbar is not defined or is not an object, using default rules.");
    return;
  }

  populateBoolConfig(workspaceTaskbar, "enable", m_enableTaskbar_);
  populateBoolConfig(workspaceTaskbar, "update-active-window", m_updateActiveWindow_);
  populateBoolConfig(workspaceTaskbar, "reverse-direction", m_taskbarReverseDirection_);

  if (workspaceTaskbar["format"].isString()) {
    /* The user defined a format string, use it */
    std::string format = workspaceTaskbar["format"].asString();
    m_taskbarWithTitle_ =
        format.find("{title") != std::string::npos; /* {title} or {title.length} */
    auto parts = split(format, "{icon}", 1);
    m_taskbarFormatBefore_ = parts[0];
    if (parts.size() > 1) {
      m_taskbarWithIcon_ = true;
      m_taskbarFormatAfter_ = parts[1];
    }
  } else {
    /* The default is to only show the icon */
    m_taskbarWithIcon_ = true;
  }

  auto iconTheme = workspaceTaskbar["icon-theme"];
  if (iconTheme.isArray()) {
    for (auto& c : iconTheme) {
      util::gtk::HIcon::add_custom_icon_theme(c.asString());
    }
  } else if (iconTheme.isString()) {
    util::gtk::HIcon::add_custom_icon_theme(iconTheme.asString());
  }

  if (workspaceTaskbar["icon-size"].isInt()) {
    m_taskbarIconSize_ = workspaceTaskbar["icon-size"].asInt();
  }
  if (workspaceTaskbar["max-icons"].isInt()) {
    m_taskbarMaxIcons_ = workspaceTaskbar["max-icons"].asInt();
  }
  if (workspaceTaskbar["orientation"].isString() &&
      toLower(workspaceTaskbar["orientation"].asString()) == "vertical") {
    m_taskbarOrientation_ = Gtk::Orientation::VERTICAL;
  }

  if (workspaceTaskbar["on-click-window"].isString()) {
    m_onClickWindow_ = workspaceTaskbar["on-click-window"].asString();
  }

  if (workspaceTaskbar["ignore-list"].isArray()) {
    for (auto& windowRegex : workspaceTaskbar["ignore-list"]) {
      std::string ruleString = windowRegex.asString();
      try {
        m_ignoreWindows_.emplace_back(ruleString, std::regex_constants::icase);
      } catch (const std::regex_error& e) {
        spdlog::error("Invalid rule {}: {}", ruleString, e.what());
      }
    }
  }

  if (workspaceTaskbar["active-window-position"].isString()) {
    auto posStr = workspaceTaskbar["active-window-position"].asString();
    try {
      m_activeWindowPosition_ =
          m_activeWindowEnumParser_.parseStringToEnum(posStr, m_activeWindowPositionMap_);
    } catch (const std::invalid_argument& e) {
      spdlog::warn(
          "Invalid string representation for active-window-position. Falling back to 'none'.");
      m_activeWindowPosition_ = ActiveWindowPosition::NONE;
    }
  }
}

void Workspaces::registerOrphanWindow(WindowCreationPayload create_window_payload) {
  if (!create_window_payload.isEmpty(*this)) {
    m_orphanWindowMap_[create_window_payload.getAddress()] = create_window_payload.repr(*this);
  }
}

auto Workspaces::registerIpc() -> void {
  m_ipc_.registerForIPC("workspacev2", this);
  m_ipc_.registerForIPC("activespecial", this);
  m_ipc_.registerForIPC("createworkspacev2", this);
  m_ipc_.registerForIPC("destroyworkspacev2", this);
  m_ipc_.registerForIPC("focusedmonv2", this);
  m_ipc_.registerForIPC("moveworkspacev2", this);
  m_ipc_.registerForIPC("renameworkspace", this);
  m_ipc_.registerForIPC("openwindow", this);
  m_ipc_.registerForIPC("closewindow", this);
  m_ipc_.registerForIPC("movewindowv2", this);
  m_ipc_.registerForIPC("urgent", this);
  m_ipc_.registerForIPC("configreloaded", this);

  if (windowRewriteConfigUsesTitle() || m_taskbarWithTitle_) {
    spdlog::info(
        "Registering for Hyprland's 'windowtitlev2' events because a user-defined window "
        "rewrite rule uses the 'title' field.");
    m_ipc_.registerForIPC("windowtitlev2", this);
  }
  if (m_updateActiveWindow_) {
    spdlog::info(
        "Registering for Hyprland's 'activewindowv2' events because 'update-active-window' is set "
        "to true.");
    m_ipc_.registerForIPC("activewindowv2", this);
  }
}

void Workspaces::removeWorkspacesToRemove() {
  for (const auto& workspaceString : m_workspacesToRemove_) {
    removeWorkspace(workspaceString);
  }
  m_workspacesToRemove_.clear();
}

void Workspaces::removeWorkspace(std::string const& workspaceString) {
  spdlog::debug("Removing workspace {}", workspaceString);

  // If this succeeds, we have a workspace ID.
  const auto workspaceId = parseWorkspaceId(workspaceString);

  std::string name;
  // TODO: At some point we want to support all workspace selectors
  // This is just a subset.
  // https://wiki.hyprland.org/Configuring/Workspace-Rules/#workspace-selectors
  if (workspaceString.starts_with("special:")) {
    name = workspaceString.substr(8);
  } else if (workspaceString.starts_with("name:")) {
    name = workspaceString.substr(5);
  } else {
    name = workspaceString;
  }

  const auto workspace = std::ranges::find_if(m_workspaces_, [&](std::unique_ptr<Workspace>& x) {
    if (workspaceId.has_value()) {
      return *workspaceId == x->id();
    }
    return name == x->name();
  });

  if (workspace == m_workspaces_.end()) {
    // happens when a workspace on another monitor is destroyed
    return;
  }

  if ((*workspace)->isPersistentConfig()) {
    spdlog::trace("Not removing config persistent workspace id={} name={}", (*workspace)->id(),
                  (*workspace)->name());
    return;
  }

  m_box_.remove(workspace->get()->button());
  m_workspaces_.erase(workspace);
}

void Workspaces::setCurrentMonitorId() {
  // get monitor ID from name (used by persistent workspaces)
  m_monitorId_ = 0;
  auto monitors = m_ipc_.getSocket1JsonReply("monitors");
  auto currentMonitor = std::ranges::find_if(monitors, [this](const Json::Value& m) {
    return m["name"].asString() == m_bar_.output->name;
  });
  if (currentMonitor == monitors.end()) {
    spdlog::error("Monitor '{}' does not have an ID? Using 0", m_bar_.output->name);
  } else {
    m_monitorId_ = (*currentMonitor)["id"].asInt();
    spdlog::trace("Current monitor ID: {}", m_monitorId_);
  }
}

void Workspaces::sortSpecialCentered() {
  std::vector<std::unique_ptr<Workspace>> specialWorkspaces;
  std::vector<std::unique_ptr<Workspace>> hiddenWorkspaces;
  std::vector<std::unique_ptr<Workspace>> normalWorkspaces;

  for (auto& workspace : m_workspaces_) {
    if (workspace->isSpecial()) {
      specialWorkspaces.push_back(std::move(workspace));
    } else {
      if (workspace->button().is_visible()) {
        normalWorkspaces.push_back(std::move(workspace));
      } else {
        hiddenWorkspaces.push_back(std::move(workspace));
      }
    }
  }
  m_workspaces_.clear();

  size_t center = normalWorkspaces.size() / 2;

  m_workspaces_.insert(m_workspaces_.end(), std::make_move_iterator(normalWorkspaces.begin()),
                       std::make_move_iterator(normalWorkspaces.begin() + center));

  m_workspaces_.insert(m_workspaces_.end(), std::make_move_iterator(specialWorkspaces.begin()),
                       std::make_move_iterator(specialWorkspaces.end()));

  m_workspaces_.insert(m_workspaces_.end(),
                       std::make_move_iterator(normalWorkspaces.begin() + center),
                       std::make_move_iterator(normalWorkspaces.end()));

  m_workspaces_.insert(m_workspaces_.end(), std::make_move_iterator(hiddenWorkspaces.begin()),
                       std::make_move_iterator(hiddenWorkspaces.end()));
}

void Workspaces::sortWorkspaces() {
  std::ranges::sort(  //
      m_workspaces_, [&](std::unique_ptr<Workspace>& a, std::unique_ptr<Workspace>& b) {
        // Helper comparisons
        auto isIdLess = a->id() < b->id();
        auto isNameLess = a->name() < b->name();

        switch (m_sortBy_) {
          case SortMethod::ID:
            return isIdLess;
          case SortMethod::NAME:
            return isNameLess;
          case SortMethod::NUMBER:
            try {
              return std::stoi(a->name()) < std::stoi(b->name());
            } catch (const std::exception& e) {
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
              // both are 0 (not yet named persistents) / named specials
              // (-98 <= ID <= -1)
              return isNameLess;
            }

            // sort non-special named workspaces by name (ID <= -1377)
            return isNameLess;
            break;
        }

        // Return a default value if none of the cases match.
        return isNameLess;  // You can adjust this to your specific needs.
      });
  if (m_sortBy_ == SortMethod::SPECIAL_CENTERED) {
    this->sortSpecialCentered();
  }

  for (size_t i{0}; i < m_workspaces_.size(); ++i) {
    auto& btn{m_workspaces_[i]->button()};
    if (i == 0)
      m_box_.reorder_child_at_start(btn);
    else
      m_box_.reorder_child_after(btn, m_workspaces_[i - 1]->button());
  }
}

void Workspaces::setUrgentWorkspace(std::string const& windowaddress) {
  const Json::Value clientsJson = m_ipc_.getSocket1JsonReply("clients");
  const std::string normalizedAddress =
      windowaddress.starts_with("0x") ? windowaddress : fmt::format("0x{}", windowaddress);
  int workspaceId = -1;

  for (const auto& clientJson : clientsJson) {
    if (clientJson["address"].asString() == normalizedAddress) {
      workspaceId = clientJson["workspace"]["id"].asInt();
      break;
    }
  }

  auto workspace = std::ranges::find_if(
      m_workspaces_,
      [workspaceId](std::unique_ptr<Workspace>& x) { return x->id() == workspaceId; });
  if (workspace != m_workspaces_.end()) {
    workspace->get()->setUrgent();
  }
}

auto Workspaces::doUpdate() -> void {
  // Debounce rapid events (e.g. out-of-order create/destroy workspace events from
  // Hyprland) to prevent workspace button flicker. This runs on the GTK main thread
  // (invoked via the dp dispatcher), so arming/disconnecting the GLib timer here is
  // thread-safe. Each event re-arms the timer, coalescing bursts into one refresh.
  if (m_debounceTimer_.connected()) {
    m_debounceTimer_.disconnect();
  }
  m_debounceTimer_ = Glib::signal_timeout().connect(
      [this]() {
        /**
         *  Workspaces::doUpdate - update workspaces in UI thread.
         *
         * Note: some memberfields are modified by both UI thread and event listener thread, use
         * m_mutex to protect these member fields, and lock should released before calling
         * AModule::update().
         */
        std::unique_lock lock(m_mutex_);
        removeWorkspacesToRemove();
        createWorkspacesToCreate();
        updateWorkspaceStates();
        updateWindowCount();
        sortWorkspaces();

        bool anyWindowCreated = updateWindowsToCreate();

        if (anyWindowCreated) {
          dp.emit();
        }

        AModule::doUpdate();
        return false;
      },
      7);
}

void Workspaces::updateWindowCount() {
  const Json::Value workspacesJson = m_ipc_.getSocket1JsonReply("workspaces");
  for (auto const& workspace : m_workspaces_) {
    auto workspaceJson = std::ranges::find_if(
        workspacesJson, [&](Json::Value const& x) { return x["id"].asInt() == workspace->id(); });
    uint32_t count = 0;
    if (workspaceJson != workspacesJson.end()) {
      try {
        count = (*workspaceJson)["windows"].asUInt();
      } catch (const std::exception& e) {
        spdlog::error("Failed to update window count: {}", e.what());
      }
    }
    workspace->setWindows(count);
  }
}

bool Workspaces::updateWindowsToCreate() {
  bool anyWindowCreated = false;
  std::vector<WindowCreationPayload> notCreated;
  for (auto& windowPayload : m_windowsToCreate_) {
    bool created = false;
    for (auto& workspace : m_workspaces_) {
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
  m_windowsToCreate_.clear();
  m_windowsToCreate_ = notCreated;
  return anyWindowCreated;
}

void Workspaces::updateWorkspaceStates() {
  const std::vector<int> visibleWorkspaces = getVisibleWorkspaces();
  auto updatedWorkspaces = m_ipc_.getSocket1JsonReply("workspaces");

  auto currentWorkspace = m_ipc_.getSocket1JsonReply("activeworkspace");
  std::string currentWorkspaceName =
      currentWorkspace.isMember("name") ? currentWorkspace["name"].asString() : "";

  for (auto& workspace : m_workspaces_) {
    bool isActiveByName =
        !currentWorkspaceName.empty() && workspace->name() == currentWorkspaceName;

    workspace->setActive(
        workspace->id() == m_activeWorkspaceId_ || isActiveByName ||
        (workspace->isSpecial() && workspace->name() == m_activeSpecialWorkspaceName_));
    if (workspace->isActive() && workspace->isUrgent()) {
      workspace->setUrgent(false);
    }
    workspace->setVisible(std::ranges::find(visibleWorkspaces, workspace->id()) !=
                          visibleWorkspaces.end());
    std::string& workspaceIcon = m_iconsMap_[""];
    if (m_withIcon_) {
      workspaceIcon = workspace->selectString(m_iconsMap_);
    }
    std::string& workspaceTooltip = m_tooltipMap_[""];
    if (m_withTooltip_) {
      workspaceTooltip = workspace->selectString(m_tooltipMap_);
    }
    auto updatedWorkspace = std::ranges::find_if(updatedWorkspaces, [&workspace](const auto& w) {
      return w["id"].asInt() == workspace->id();
    });
    if (updatedWorkspace != updatedWorkspaces.end()) {
      workspace->setOutput((*updatedWorkspace)["monitor"].asString());
    }
    workspace->doUpdate(workspaceIcon, workspaceTooltip);
  }
}

int Workspaces::windowRewritePriorityFunction(std::string const& window_rule) {
  // Rules that match against title are prioritized
  // Rules that don't specify if they're matching against either title or class are deprioritized
  bool const hasTitle = window_rule.find("title") != std::string::npos;
  bool const hasClass = window_rule.find("class") != std::string::npos;

  if (hasTitle && hasClass) {
    m_anyWindowRewriteRuleUsesTitle_ = true;
    return 3;
  }
  if (hasTitle) {
    m_anyWindowRewriteRuleUsesTitle_ = true;
    return 2;
  }
  if (hasClass) {
    return 1;
  }
  return 0;
}

template <typename... Args>
std::string Workspaces::makePayload(Args const&... args) {
  std::ostringstream result;
  bool first = true;
  ((result << (first ? "" : ",") << args, first = false), ...);
  return result.str();
}

std::pair<std::string, std::string> Workspaces::splitDoublePayload(std::string const& payload) {
  const auto separator = payload.find(',');
  if (separator == std::string::npos) {
    throw std::invalid_argument("Expected a two-part Hyprland payload");
  }
  const std::string part1 = payload.substr(0, separator);
  const std::string part2 = payload.substr(part1.size() + 1);
  return {part1, part2};
}

std::tuple<std::string, std::string, std::string> Workspaces::splitTriplePayload(
    std::string const& payload) {
  const size_t firstComma = payload.find(',');
  const size_t secondComma = payload.find(',', firstComma + 1);
  if (firstComma == std::string::npos || secondComma == std::string::npos) {
    throw std::invalid_argument("Expected a three-part Hyprland payload");
  }

  const std::string part1 = payload.substr(0, firstComma);
  const std::string part2 = payload.substr(firstComma + 1, secondComma - (firstComma + 1));
  const std::string part3 = payload.substr(secondComma + 1);

  return {part1, part2, part3};
}

std::optional<int> Workspaces::parseWorkspaceId(std::string const& workspaceIdStr) {
  try {
    return workspaceIdStr == "special" ? -99 : std::stoi(workspaceIdStr);
  } catch (std::exception const& e) {
    spdlog::debug("Workspace \"{}\" is not bound to an id: {}", workspaceIdStr, e.what());
    return std::nullopt;
  }
}

bool Workspaces::handleScroll(double dx, double dy) {
  const auto e{controller_scroll_->get_current_event()};
  // Ignore emulated scroll events on window
  if (auto device{e->get_device()}) {
    if (device->get_source() == Gdk::InputSource::TOUCHSCREEN) {
      return false;
    }
  }
  // Check for custom scroll commands first; delegate to base class
  if (config_["on-scroll-up"].isString() || config_["on-scroll-down"].isString()) {
    return AModule::handleScroll(dx, dy);
  }

  auto dir = AModule::getScrollDir(e);
  if (dir == SCROLL_DIR::NONE) {
    return true;
  }

  if (dir == SCROLL_DIR::DOWN || dir == SCROLL_DIR::RIGHT) {
    if (allOutputs()) {
      IPC::dispatch("workspace", "e+1");
    } else {
      IPC::dispatch("workspace", "m+1");
    }
  } else if (dir == SCROLL_DIR::UP || dir == SCROLL_DIR::LEFT) {
    if (allOutputs()) {
      IPC::dispatch("workspace", "e-1");
    } else {
      IPC::dispatch("workspace", "m-1");
    }
  }

  return true;
}

}  // namespace waybar::modules::hyprland
