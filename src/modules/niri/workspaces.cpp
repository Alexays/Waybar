#include "modules/niri/workspaces.hpp"

#include <fmt/ranges.h>  // Needed for joining window representations
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>

#include "util/rewrite_string.hpp"  // Needed for rewrite logic

namespace waybar::modules::niri {

Workspaces::Workspaces(const std::string& id, const Bar& bar, const Json::Value& config,
                       std::mutex& reap_mtx, std::list<pid_t>& reap)
    : AModule(config, "workspaces", id, reap_mtx, reap, false, false),
      bar_(bar),
      box_(bar.orientation, 0) {
  const auto config_sort_by_number = config_["sort-by-number"];
  if (config_sort_by_number.isBool()) {
    spdlog::warn("[niri/workspaces]: Prefer sort-by-id instead of sort-by-number");
    sort_by_id_ = config_sort_by_number.asBool();
  }

  const auto config_sort_by_id = config_["sort-by-id"];
  if (config_sort_by_id.isBool()) {
    sort_by_id_ = config_sort_by_id.asBool();
  }

  const auto config_sort_by_name = config_["sort-by-name"];
  if (config_sort_by_name.isBool()) {
    sort_by_name_ = config_sort_by_name.asBool();
  }

  const auto config_sort_by_coordinates = config_["sort-by-coordinates"];
  if (config_sort_by_coordinates.isBool()) {
    sort_by_coordinates_ = config_sort_by_coordinates.asBool();
  }

  box_.set_name("workspaces");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(MODULE_CLASS);
  event_box_.add(box_);

  // Parse ignore-workspaces rules.
  auto ignoreWorkspaces = config["ignore-workspaces"];
  if (ignoreWorkspaces.isArray()) {
    for (const auto& workspaceRegex : ignoreWorkspaces) {
      if (workspaceRegex.isString()) {
        std::string ruleString = workspaceRegex.asString();
        try {
          const std::regex rule{ruleString, std::regex_constants::icase};
          ignoreWorkspaces_.emplace_back(rule);
        } catch (const std::regex_error& e) {
          spdlog::error("Invalid ignore-workspaces rule {}: {}", ruleString, e.what());
        }
      } else {
        spdlog::error("Not a string in ignore-workspaces: '{}'", workspaceRegex);
      }
    }
  }

  if (!gIPC) gIPC = std::make_unique<IPC>();

  // Parse new config options first
  populateWindowRewriteConfig();
  populateFormatWindowSeparatorConfig();

  // Niri's WorkspacesChanged event already includes window info,
  // so no need to register for separate window events like in Hyprland.
  gIPC->registerForIPC("WorkspacesChanged", this);
  gIPC->registerForIPC("WorkspaceActivated", this);
  gIPC->registerForIPC("WorkspaceActiveWindowChanged", this);
  gIPC->registerForIPC("WorkspaceUrgencyChanged", this);

  gIPC->registerForIPC("WindowsChanged", this);
  gIPC->registerForIPC("WindowOpenedOrChanged", this);
  gIPC->registerForIPC("WindowLayoutsChanged", this);
  gIPC->registerForIPC("WindowFocusChanged", this);
  gIPC->registerForIPC("WindowClosed", this);

  if (config["enable-bar-scroll"].asBool()) {
    auto& window = const_cast<Bar&>(bar_).window;
    window.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
    window.signal_scroll_event().connect(sigc::mem_fun(*this, &Workspaces::handleScroll));
  }
}

Workspaces::~Workspaces() { gIPC->unregisterForIPC(this); }

void Workspaces::onEvent(const Json::Value& /*ev*/) { dp.emit(); }

void Workspaces::doUpdate() {
  auto ipcLock = gIPC->lockData();

  // Debug: log global IPC lists
  spdlog::debug("[niri/workspaces] gIPC workspaces count: {}", gIPC->workspaces().size());
  spdlog::debug("[niri/workspaces] gIPC windows count: {}", gIPC->windows().size());

  const auto alloutputs = config_["all-outputs"].asBool();
  const auto display_cond = config_["display-condition"].asString();
  const auto& all_workspaces = gIPC->workspaces();
  const auto& all_windows = gIPC->windows();

  std::vector<const Json::Value*> my_workspaces;
  my_workspaces.reserve(all_workspaces.size());
  for (const auto& ws : all_workspaces) {
    std::string name;
    if (ws["name"]) {
      name = ws["name"].asString();
    } else {
      name = std::to_string(ws["idx"].asUInt());
    }
    if (isWorkspaceIgnored(name)) {
      continue;
    }

    if (display_cond == "only-populated") {
      if (ws["active_window_id"].isNull() && !ws["is_active"].asBool()) continue;
    } else if (display_cond == "keep-named") {
      if (ws["name"].isNull() && ws["active_window_id"].isNull() && !ws["is_active"].asBool())
        continue;
    }

    if (alloutputs || ws["output"].asString() == bar_.output->name) {
      my_workspaces.push_back(&ws);
    }
  }

  sortWorkspaces(my_workspaces);

  workspaces_.erase(std::remove_if(workspaces_.begin(), workspaces_.end(),
                                   [&](const std::unique_ptr<Workspace>& w) {
                                     bool gone = std::none_of(
                                         my_workspaces.begin(), my_workspaces.end(),
                                         [&](const Json::Value* ws) {
                                           return ws->operator[]("id").asUInt64() == w->id();
                                         });
                                     if (gone) box_.remove(w->button());
                                     return gone;
                                   }),
                    workspaces_.end());

  for (const auto* ws_ptr : my_workspaces) {
    const auto& ws = *ws_ptr;
    const auto ws_id = ws.isMember("id") ? ws["id"].asUInt64() : 0;

    auto it =
        std::find_if(workspaces_.begin(), workspaces_.end(),
                     [ws_id](const std::unique_ptr<Workspace>& w) { return w->id() == ws_id; });

    if (it == workspaces_.end()) {
      createWorkspace(ws);
      it = workspaces_.end() - 1;
    }

    std::vector<Json::Value> windows_vec(all_windows.begin(), all_windows.end());
    const auto windows_str = getWindowsRepresentation(ws);
    (*it)->update(ws, windows_vec, windows_str, my_workspaces.size());
  }

  for (auto pos_it = my_workspaces.cbegin(); pos_it != my_workspaces.cend(); ++pos_it) {
    const auto& ws = **pos_it;
    const auto ws_id = ws.isMember("id") ? ws["id"].asUInt64() : 0;

    const auto pos = static_cast<int>(std::distance(my_workspaces.cbegin(), pos_it));

    auto it =
        std::find_if(workspaces_.begin(), workspaces_.end(),
                     [ws_id](const std::unique_ptr<Workspace>& w) { return w->id() == ws_id; });
    if (it != workspaces_.end()) {
      box_.reorder_child((*it)->button(), pos);
    }
  }
}

void Workspaces::update() {
  doUpdate();
  AModule::update();
}

void Workspaces::createWorkspace(const Json::Value& workspace_data) {
  auto ws = std::make_unique<Workspace>(workspace_data, *this);
  box_.pack_start(ws->button(), false, false, 0);
  workspaces_.push_back(std::move(ws));
}

// --- Start New Helper Functions ---
void Workspaces::populateWindowRewriteConfig() {
  // Reconstruct RegexCollection instead of clearing/adding
  const Json::Value& rewrite_rules_config = config_["window-rewrite"];
  if (rewrite_rules_config.isObject()) {
    // Assuming a constructor that takes the Json::Value object exists.
    // If Niri needs rule prioritization like Hyprland, a priority function
    // would be needed as a second argument here.
    try {
      m_windowRewriteRules = util::RegexCollection(rewrite_rules_config);
    } catch (const std::exception& e) {
      spdlog::error("Error initializing RegexCollection: {}", e.what());
      // Initialize with an empty collection if error occurs
      m_windowRewriteRules = util::RegexCollection(Json::Value(Json::objectValue));
    }

  } else {
    // Initialize with an empty collection if config is not an object
    m_windowRewriteRules = util::RegexCollection(Json::Value(Json::objectValue));
  }

  if (config_.isMember("window-rewrite-default") && config_["window-rewrite-default"].isString()) {
    m_windowRewriteDefault = config_["window-rewrite-default"].asString();
  } else {
    m_windowRewriteDefault = "?";  // Default fallback
  }
}

void Workspaces::populateFormatWindowSeparatorConfig() {
  if (config_.isMember("format-window-separator") &&
      config_["format-window-separator"].isString()) {
    m_formatWindowSeparator = config_["format-window-separator"].asString();
  } else {
    m_formatWindowSeparator = " ";  // Default fallback
  }
}

std::string Workspaces::getRewrite(const std::string& app_id, const std::string& title) {
  // Niri uses app_id, Hyprland uses class. Adapt the key format.
  std::string lookup_key = "app_id<" + app_id + "> title<" + title + ">";
  std::string res = m_windowRewriteRules.get(lookup_key);
  if (!res.empty()) {
    // Create Json::Value for substitutions
    Json::Value substitutions(Json::objectValue);
    substitutions["app_id"] = app_id;
    substitutions["title"] = title;
    return util::rewriteString(res, substitutions);
  }
  // Fallback to app_id only
  lookup_key = "app_id<" + app_id + ">";
  res = m_windowRewriteRules.get(lookup_key);
  if (!res.empty()) {
    // Create Json::Value for substitutions
    Json::Value substitutions(Json::objectValue);
    substitutions["app_id"] = app_id;
    substitutions["title"] = title;
    return util::rewriteString(res, substitutions);
  }
  // Fallback to title only
  lookup_key = "title<" + title + ">";
  res = m_windowRewriteRules.get(lookup_key);
  if (!res.empty()) {
    // Create Json::Value for substitutions
    Json::Value substitutions(Json::objectValue);
    substitutions["app_id"] = app_id;
    substitutions["title"] = title;
    return util::rewriteString(res, substitutions);
  }

  // No rule matched, return default
  // Apply substitutions to default as well, in case it uses placeholders
  Json::Value substitutions(Json::objectValue);
  substitutions["app_id"] = app_id;
  substitutions["title"] = title;
  return util::rewriteString(m_windowRewriteDefault, substitutions);
}

// Build the "{windows}" replacement string for a workspace using window-rewrite rules.
std::string Workspaces::getWindowsRepresentation(const Json::Value& ws) {
  std::vector<std::string> window_reps;
  auto collect = [&](const Json::Value& win) {
    std::string app_id =
        win.isMember("app_id") && win["app_id"].isString() ? win["app_id"].asString() : "";
    std::string title =
        win.isMember("title") && win["title"].isString() ? win["title"].asString() : "";
    if (!app_id.empty() || !title.empty()) {
      window_reps.push_back(getRewrite(app_id, title));
    }
  };

  if (ws.isMember("windows") && ws["windows"].isArray()) {
    for (const auto& win : ws["windows"]) {
      collect(win);
    }
  } else {
    // Fallback: collect from global windows list by matching workspace_id.
    for (const auto& win : gIPC->windows()) {
      if (!win.isMember("workspace_id")) continue;
      if (win["workspace_id"].asUInt64() != ws["id"].asUInt64()) continue;
      collect(win);
    }
  }

  return fmt::format("{}", fmt::join(window_reps, m_formatWindowSeparator));
}
// --- End New Helper Functions ---

std::string Workspaces::getIcon(const std::string& value, const Json::Value& ws) const {
  const auto& icons = config_["format-icons"];
  if (!icons) return value;

  if (ws["is_urgent"].asBool() && icons["urgent"]) return icons["urgent"].asString();
  if (ws["is_active"].asBool() && icons["active"]) return icons["active"].asString();
  if (ws["is_focused"].asBool() && icons["focused"]) return icons["focused"].asString();
  if (ws["active_window_id"].isNull() && icons["empty"]) return icons["empty"].asString();

  if (ws["name"]) {
    const auto& name = ws["name"].asString();
    if (icons[name]) return icons[name].asString();
  }

  const auto idx = ws["idx"].asString();
  if (icons[idx]) return icons[idx].asString();

  if (icons["default"]) return icons["default"].asString();

  return value;
}

bool Workspaces::isWorkspaceIgnored(const std::string& name) {
  for (auto& rule : ignoreWorkspaces_) {
    if (std::regex_match(name, rule)) {
      return true;
    }
  }
  return false;
}

bool Workspaces::handleScroll(GdkEventScroll* e) {
  if (gdk_event_get_pointer_emulated((GdkEvent*)e) != 0) {
    /**
     * Ignore emulated scroll events on window
     */
    return false;
  }

  auto dir = AModule::getScrollDir(e);
  if (dir == SCROLL_DIR::NONE) {
    return true;
  }

  try {
    Json::Value request(Json::objectValue);
    auto& action = (request["Action"] = Json::Value(Json::objectValue));

    std::string action_name;

    if (dir == SCROLL_DIR::DOWN || dir == SCROLL_DIR::RIGHT) {
      action_name = "FocusWorkspaceDown";
    } else if (dir == SCROLL_DIR::UP || dir == SCROLL_DIR::LEFT) {
      action_name = "FocusWorkspaceUp";
    } else {
      return true;
    }

    action[action_name] = Json::Value(Json::objectValue);

    IPC::send(request);

  } catch (const std::exception& e) {
    spdlog::error("Workspaces: {}", e.what());
    return false;
  }

  return true;
}

void Workspaces::sortWorkspaces(std::vector<const Json::Value*>& workspaces) const {
  auto get_name = [](const Json::Value& ws) -> std::string {
    if (ws["name"]) return ws["name"].asString();
    return std::to_string(ws["idx"].asUInt());
  };

  auto is_numeric = [](const std::string& value) {
    return !value.empty() &&
           std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isdigit(c); });
  };

  const bool names_are_numeric =
      std::all_of(workspaces.begin(), workspaces.end(),
                  [&](const auto* ws) { return is_numeric(get_name(*ws)); });

  auto compare_numeric_strings = [](const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return a.size() < b.size();
    return a < b;
  };

  std::sort(workspaces.begin(), workspaces.end(), [&](const auto* ap, const auto* bp) {
    const auto& a = *ap;
    const auto& b = *bp;
    if (sort_by_id_) {
      return a["id"].asUInt64() < b["id"].asUInt64();
    }

    if (sort_by_name_) {
      const auto a_name = get_name(a);
      const auto b_name = get_name(b);
      if (a_name == b_name) return a["id"].asUInt64() < b["id"].asUInt64();
      if (names_are_numeric) return compare_numeric_strings(a_name, b_name);
      return a_name < b_name;
    }

    if (sort_by_coordinates_) {
      const auto& a_output = a["output"].asString();
      const auto& b_output = b["output"].asString();
      if (a_output == b_output) {
        const auto a_idx = a["idx"].asUInt();
        const auto b_idx = b["idx"].asUInt();
        if (a_idx == b_idx) return a["id"].asUInt64() < b["id"].asUInt64();
        return a_idx < b_idx;
      }
      return a_output < b_output;
    }

    // Default to sorting by workspace index on each output.
    const auto& a_output = a["output"].asString();
    const auto& b_output = b["output"].asString();
    const auto a_idx = a["idx"].asUInt();
    const auto b_idx = b["idx"].asUInt();
    if (a_output == b_output) return a_idx < b_idx;
    return a_output < b_output;
  });
}

}  // namespace waybar::modules::niri
