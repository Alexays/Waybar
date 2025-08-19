#include "modules/niri/workspaces.hpp"

#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <spdlog/spdlog.h>
#include <fmt/ranges.h> // Needed for joining window representations

#include "util/rewrite_string.hpp" // Needed for rewrite logic

namespace waybar::modules::niri {

Workspaces::Workspaces(const std::string &id, const Bar &bar, const Json::Value &config)
    : AModule(config, "workspaces", id, false, false), bar_(bar), box_(bar.orientation, 0) {
  box_.set_name("workspaces");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(MODULE_CLASS);
  event_box_.add(box_);

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

  dp.emit();
}

Workspaces::~Workspaces() { gIPC->unregisterForIPC(this); }

void Workspaces::onEvent(const Json::Value &ev) { dp.emit(); }

void Workspaces::doUpdate() {
  auto ipcLock = gIPC->lockData();

  const auto alloutputs = config_["all-outputs"].asBool();
  std::vector<Json::Value> my_workspaces;
  const auto &workspaces = gIPC->workspaces();
  std::copy_if(workspaces.cbegin(), workspaces.cend(), std::back_inserter(my_workspaces),
               [&](const auto &ws) {
                 if (alloutputs) return true;
                 return ws["output"].asString() == bar_.output->name;
               });

  // Remove buttons for removed workspaces.
  for (auto it = buttons_.begin(); it != buttons_.end();) {
    auto ws = std::find_if(my_workspaces.begin(), my_workspaces.end(),
                           [it](const auto &ws) { return ws["id"].asUInt64() == it->first; });
    if (ws == my_workspaces.end()) {
      it = buttons_.erase(it);
    } else {
      ++it;
    }
  }

  // Add buttons for new workspaces, update existing ones.
  for (const auto &ws : my_workspaces) {
    auto bit = buttons_.find(ws["id"].asUInt64());
    auto &button = bit == buttons_.end() ? addButton(ws) : bit->second;
    auto style_context = button.get_style_context();

    if (ws["is_focused"].asBool())
      style_context->add_class("focused");
    else
      style_context->remove_class("focused");

    if (ws["is_active"].asBool())
      style_context->add_class("active");
    else
      style_context->remove_class("active");

    if (ws["is_urgent"].asBool())
      style_context->add_class("urgent");
    else
      style_context->remove_class("urgent");

    if (ws["output"]) {
      if (ws["output"].asString() == bar_.output->name)
        style_context->add_class("current_output");
      else
        style_context->remove_class("current_output");
    } else {
      style_context->remove_class("current_output");
    }

    if (ws["active_window_id"].isNull())
      style_context->add_class("empty");
    else
      style_context->remove_class("empty");

    // --- Start Window Rewrite Logic ---
    std::vector<std::string> window_reps;
    if (ws.isMember("windows") && ws["windows"].isArray()) {
      for (const auto &win : ws["windows"]) {
        // Niri provides app_id and title directly in the window object
        std::string app_id = win.isMember("app_id") && win["app_id"].isString() ? win["app_id"].asString() : "";
        std::string title = win.isMember("title") && win["title"].isString() ? win["title"].asString() : "";
        if (!app_id.empty() || !title.empty()) { // Only add if we have some identifier
           window_reps.push_back(getRewrite(app_id, title));
        }
      }
    }
    // Join representations with the separator
    auto windows_str = fmt::format("{}", fmt::join(window_reps, m_formatWindowSeparator));
    // --- End Window Rewrite Logic ---


    std::string name;
    if (ws["name"]) {
      name = ws["name"].asString();
    } else {
      name = std::to_string(ws["idx"].asUInt());
    }
    button.set_name("niri-workspace-" + name);

    if (config_["format"].isString()) {
      auto format = config_["format"].asString();
      // Add "windows" argument to format call
      name = fmt::format(fmt::runtime(format), fmt::arg("icon", getIcon(name, ws)),
                         fmt::arg("value", name), fmt::arg("name", ws["name"].asString()),
                         fmt::arg("index", ws["idx"].asUInt()),
                         fmt::arg("output", ws["output"].asString()),
                         fmt::arg("windows", windows_str)); // Added windows arg
    }
    if (!config_["disable-markup"].asBool()) {
      static_cast<Gtk::Label *>(button.get_children()[0])->set_markup(name);
    } else {
      button.set_label(name);
    }

    if (config_["current-only"].asBool()) {
      const auto *property = alloutputs ? "is_focused" : "is_active";
      if (ws[property].asBool())
        button.show();
      else
        button.hide();
    } else {
      button.show();
    }
  }

  // Refresh the button order.
  for (auto it = my_workspaces.cbegin(); it != my_workspaces.cend(); ++it) {
    const auto &ws = *it;

    auto pos = ws["idx"].asUInt() - 1;
    if (alloutputs) pos = it - my_workspaces.cbegin();

    auto &button = buttons_[ws["id"].asUInt64()];
    box_.reorder_child(button, pos);
  }
}

void Workspaces::update() {
  doUpdate();
  AModule::update();
}

Gtk::Button &Workspaces::addButton(const Json::Value &ws) {
  std::string name;
  if (ws["name"]) {
    name = ws["name"].asString();
  } else {
    name = std::to_string(ws["idx"].asUInt());
  }

  auto pair = buttons_.emplace(ws["id"].asUInt64(), name);
  auto &&button = pair.first->second;
  box_.pack_start(button, false, false, 0);
  button.set_relief(Gtk::RELIEF_NONE);
  if (!config_["disable-click"].asBool()) {
    const auto id = ws["id"].asUInt64();
    button.signal_pressed().connect([=] {
      try {
        // {"Action":{"FocusWorkspace":{"reference":{"Id":1}}}}
        Json::Value request(Json::objectValue);
        auto &action = (request["Action"] = Json::Value(Json::objectValue));
        auto &focusWorkspace = (action["FocusWorkspace"] = Json::Value(Json::objectValue));
        auto &reference = (focusWorkspace["reference"] = Json::Value(Json::objectValue));
        reference["Id"] = id;

        IPC::send(request);
      } catch (const std::exception &e) {
        spdlog::error("Error switching workspace: {}", e.what());
      }
    });
  }
  return button;
}

// --- Start New Helper Functions ---
void Workspaces::populateWindowRewriteConfig() {
  // Reconstruct RegexCollection instead of clearing/adding
  const Json::Value &rewrite_rules_config = config_["window-rewrite"];
  if (rewrite_rules_config.isObject()) {
      // Assuming a constructor that takes the Json::Value object exists.
      // If Niri needs rule prioritization like Hyprland, a priority function
      // would be needed as a second argument here.
      try {
          m_windowRewriteRules = util::RegexCollection(rewrite_rules_config);
      } catch (const std::exception &e) {
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
      m_windowRewriteDefault = "?"; // Default fallback
  }
}

void Workspaces::populateFormatWindowSeparatorConfig() {
  if (config_.isMember("format-window-separator") && config_["format-window-separator"].isString()) {
      m_formatWindowSeparator = config_["format-window-separator"].asString();
  } else {
     m_formatWindowSeparator = " "; // Default fallback
  }
}

std::string Workspaces::getRewrite(const std::string &app_id, const std::string &title) {
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
// --- End New Helper Functions ---


std::string Workspaces::getIcon(const std::string &value, const Json::Value &ws) {
  const auto &icons = config_["format-icons"];
  if (!icons) return value;

  if (ws["is_urgent"].asBool() && icons["urgent"]) return icons["urgent"].asString();

  if (ws["active_window_id"].isNull() && icons["empty"]) return icons["empty"].asString();

  if (ws["is_focused"].asBool() && icons["focused"]) return icons["focused"].asString();

  if (ws["is_active"].asBool() && icons["active"]) return icons["active"].asString();

  if (ws["name"]) {
    const auto &name = ws["name"].asString();
    if (icons[name]) return icons[name].asString();
  }

  const auto idx = ws["idx"].asString();
  if (icons[idx]) return icons[idx].asString();

  if (icons["default"]) return icons["default"].asString();

  return value;
}

}  // namespace waybar::modules::niri
