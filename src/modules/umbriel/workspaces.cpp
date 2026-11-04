#include "modules/umbriel/workspaces.hpp"

#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <string>

#include "util/rewrite_string.hpp"

namespace waybar::modules::umbriel {

Workspaces::Workspaces(const std::string& id, const Bar& bar, const Json::Value& config)
    : AModule(config, "workspaces", id, false, false), bar_(bar), box_(bar.orientation, 0) {
  const auto config_sort_by_id = config_["sort-by-id"];
  if (config_sort_by_id.isBool()) {
    sort_by_id_ = config_sort_by_id.asBool();
  }

  const auto config_sort_by_name = config_["sort-by-name"];
  if (config_sort_by_name.isBool()) {
    sort_by_name_ = config_sort_by_name.asBool();
  }

  box_.set_name("workspaces");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(MODULE_CLASS);
  event_box_.add(box_);

  // Parse ignore-workspaces rules.
  const auto ignore_workspaces = config["ignore-workspaces"];
  if (ignore_workspaces.isArray()) {
    for (const auto& workspace_regex : ignore_workspaces) {
      if (workspace_regex.isString()) {
        const auto rule_string = workspace_regex.asString();
        try {
          ignoreWorkspaces_.emplace_back(rule_string, std::regex_constants::icase);
        } catch (const std::regex_error& e) {
          spdlog::error("Invalid ignore-workspaces rule {}: {}", rule_string, e.what());
        }
      } else {
        spdlog::error("Not a string in ignore-workspaces: '{}'", workspace_regex);
      }
    }
  }

  if (!gIPC) gIPC = std::make_unique<IPC>();

  populateWindowRewriteConfig();
  populateFormatWindowSeparatorConfig();

  gIPC->registerForIPC("workspaces", this);
  gIPC->registerForIPC("windows", this);

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

  const auto alloutputs = config_["all-outputs"].asBool();
  const auto display_cond = config_["display-condition"].asString();
  const auto& all_workspaces = gIPC->workspaces();
  const auto& all_windows = gIPC->windows();

  std::vector<const Json::Value*> my_workspaces;
  my_workspaces.reserve(all_workspaces.size());
  for (const auto& ws : all_workspaces) {
    const auto name = ws["name"].asString();
    if (isWorkspaceIgnored(name)) {
      continue;
    }

    const auto active = ws["active"].asBool();
    const auto named = ws["named"].asBool();
    const auto window_count = ws["window_count"].asInt();

    if (display_cond == "only-populated") {
      if (window_count == 0 && !active) continue;
    } else if (display_cond == "keep-named") {
      if (!named && window_count == 0 && !active) continue;
    }

    if (alloutputs || ws["output"].asString() == bar_.output->name) {
      my_workspaces.push_back(&ws);
    }
  }

  sortWorkspaces(my_workspaces);

  workspaces_.erase(std::remove_if(workspaces_.begin(), workspaces_.end(),
                                   [&](const std::unique_ptr<Workspace>& w) {
                                     const bool gone = std::none_of(
                                         my_workspaces.begin(), my_workspaces.end(),
                                         [&](const Json::Value* ws) {
                                           return ws->operator[]("id").asString() == w->id();
                                         });
                                     if (gone) box_.remove(w->button());
                                     return gone;
                                   }),
                    workspaces_.end());

  for (const auto* ws_ptr : my_workspaces) {
    const auto& ws = *ws_ptr;
    const auto ws_id = ws["id"].asString();

    auto it =
        std::find_if(workspaces_.begin(), workspaces_.end(),
                     [&ws_id](const std::unique_ptr<Workspace>& w) { return w->id() == ws_id; });

    if (it == workspaces_.end()) {
      createWorkspace(ws);
      it = workspaces_.end() - 1;
    }

    const auto windows_str = getWindowsRepresentation(ws, all_windows);
    (*it)->update(ws, all_windows, windows_str, my_workspaces.size());
  }

  for (auto pos_it = my_workspaces.cbegin(); pos_it != my_workspaces.cend(); ++pos_it) {
    const auto ws_id = (*pos_it)->operator[]("id").asString();
    const auto pos = static_cast<int>(std::distance(my_workspaces.cbegin(), pos_it));

    auto it =
        std::find_if(workspaces_.begin(), workspaces_.end(),
                     [&ws_id](const std::unique_ptr<Workspace>& w) { return w->id() == ws_id; });
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

void Workspaces::populateWindowRewriteConfig() {
  const auto& rewrite_rules_config = config_["window-rewrite"];
  if (rewrite_rules_config.isObject()) {
    try {
      m_windowRewriteRules = util::RegexCollection(rewrite_rules_config);
    } catch (const std::exception& e) {
      spdlog::error("Error initializing window-rewrite rules: {}", e.what());
      m_windowRewriteRules = util::RegexCollection(Json::Value(Json::objectValue));
    }
  } else {
    m_windowRewriteRules = util::RegexCollection(Json::Value(Json::objectValue));
  }

  if (config_.isMember("window-rewrite-default") && config_["window-rewrite-default"].isString()) {
    m_windowRewriteDefault = config_["window-rewrite-default"].asString();
  } else {
    m_windowRewriteDefault = "?";
  }
}

void Workspaces::populateFormatWindowSeparatorConfig() {
  if (config_.isMember("format-window-separator") &&
      config_["format-window-separator"].isString()) {
    m_formatWindowSeparator = config_["format-window-separator"].asString();
  } else {
    m_formatWindowSeparator = " ";
  }
}

std::string Workspaces::getRewrite(const std::string& app_id, const std::string& title) {
  Json::Value substitutions(Json::objectValue);
  substitutions["app_id"] = app_id;
  substitutions["title"] = title;

  std::string lookup_key = "app_id<" + app_id + "> title<" + title + ">";
  std::string res = m_windowRewriteRules.get(lookup_key);
  if (!res.empty()) {
    return util::rewriteString(res, substitutions);
  }

  lookup_key = "app_id<" + app_id + ">";
  res = m_windowRewriteRules.get(lookup_key);
  if (!res.empty()) {
    return util::rewriteString(res, substitutions);
  }

  lookup_key = "title<" + title + ">";
  res = m_windowRewriteRules.get(lookup_key);
  if (!res.empty()) {
    return util::rewriteString(res, substitutions);
  }

  return util::rewriteString(m_windowRewriteDefault, substitutions);
}

std::string Workspaces::getWindowsRepresentation(const Json::Value& ws,
                                                 const std::vector<Json::Value>& all_windows) {
  const auto workspace_id = ws["id"].asString();
  std::vector<std::string> window_reps;

  for (const auto& win : all_windows) {
    if (win["workspace"].asString() != workspace_id) {
      continue;
    }

    const auto app_id = win["app_id"].isString() ? win["app_id"].asString() : "";
    const auto title = win["title"].isString() ? win["title"].asString() : "";
    if (!app_id.empty() || !title.empty()) {
      window_reps.push_back(getRewrite(app_id, title));
    }
  }

  return fmt::format("{}", fmt::join(window_reps, m_formatWindowSeparator));
}

std::string Workspaces::getIcon(const std::string& value, const Json::Value& ws) const {
  const auto& icons = config_["format-icons"];
  if (!icons) return value;

  if (ws["urgent"].asBool() && icons["urgent"]) return icons["urgent"].asString();
  if (ws["active"].asBool() && icons["active"]) return icons["active"].asString();
  if (ws["focused"].asBool() && icons["focused"]) return icons["focused"].asString();
  if (ws["window_count"].asInt() == 0 && icons["empty"]) return icons["empty"].asString();

  if (ws["name"].isString()) {
    const auto& name = ws["name"].asString();
    if (icons[name]) return icons[name].asString();
  }

  const auto index = std::to_string(ws["index"].asUInt());
  if (icons[index]) return icons[index].asString();

  if (icons["default"]) return icons["default"].asString();

  return value;
}

bool Workspaces::isWorkspaceIgnored(const std::string& name) const {
  for (const auto& rule : ignoreWorkspaces_) {
    if (std::regex_match(name, rule)) {
      return true;
    }
  }
  return false;
}

bool Workspaces::handleScroll(GdkEventScroll* e) {
  if (gdk_event_get_pointer_emulated(reinterpret_cast<GdkEvent*>(e)) != 0) {
    return false;
  }

  const auto dir = AModule::getScrollDir(e);
  if (dir == SCROLL_DIR::NONE) {
    return true;
  }

  try {
    Json::Value request(Json::objectValue);
    request["cmd"] = "action";

    const bool reverse_scroll = config_["reverse-scroll"].asBool();
    const bool down = dir == SCROLL_DIR::DOWN || dir == SCROLL_DIR::RIGHT;
    const bool up = dir == SCROLL_DIR::UP || dir == SCROLL_DIR::LEFT;

    if (reverse_scroll ? up : down) {
      request["action"] = "workspace-next";
    } else if (reverse_scroll ? down : up) {
      request["action"] = "workspace-previous";
    } else {
      return true;
    }

    IPC::send(request);
  } catch (const std::exception& e) {
    spdlog::error("Umbriel workspaces: {}", e.what());
    return false;
  }

  return true;
}

void Workspaces::sortWorkspaces(std::vector<const Json::Value*>& workspaces) const {
  const auto get_name = [](const Json::Value& ws) -> std::string {
    if (ws["name"].isString()) return ws["name"].asString();
    return std::to_string(ws["index"].asUInt());
  };

  const auto is_numeric = [](const std::string& value) {
    return !value.empty() &&
           std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isdigit(c); });
  };

  const bool names_are_numeric =
      std::all_of(workspaces.begin(), workspaces.end(),
                  [&](const auto* ws) { return is_numeric(get_name(*ws)); });

  const auto compare_numeric_strings = [](const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return a.size() < b.size();
    return a < b;
  };

  std::sort(workspaces.begin(), workspaces.end(), [&](const auto* ap, const auto* bp) {
    const auto& a = *ap;
    const auto& b = *bp;

    if (sort_by_id_) {
      const auto a_id = a["id"].asString();
      const auto b_id = b["id"].asString();
      if (a_id != b_id) return a_id < b_id;
    }

    if (sort_by_name_) {
      const auto a_name = get_name(a);
      const auto b_name = get_name(b);
      if (a_name != b_name) {
        return names_are_numeric ? compare_numeric_strings(a_name, b_name) : a_name < b_name;
      }
    }

    const auto& a_output = a["output"].asString();
    const auto& b_output = b["output"].asString();
    if (a_output != b_output) return a_output < b_output;

    const auto a_index = a["index"].asUInt();
    const auto b_index = b["index"].asUInt();
    if (a_index != b_index) return a_index < b_index;

    return a["id"].asString() < b["id"].asString();
  });
}

}  // namespace waybar::modules::umbriel
