#include "modules/hyprland/workspaces.hpp"

#include <json/value.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <charconv>
#include <memory>
#include <string>

namespace waybar::modules::hyprland {

Workspaces::Workspaces(const std::string &id, const Bar &bar, const Json::Value &config)
    : AModule(config, "workspaces", id, false, false),
      bar_(bar),
      box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0) {
  parse_config(config);

  box_.set_name("workspaces");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  event_box_.add(box_);

  register_ipc();

  init();
}

auto Workspaces::parse_config(const Json::Value &config) -> void {
  Json::Value config_format = config["format"];

  format_ = config_format.isString() ? config_format.asString() : "{name}";
  with_icon_ = format_.find("{icon}") != std::string::npos;

  if (with_icon_ && icons_map_.empty()) {
    Json::Value format_icons = config["format-icons"];
    for (std::string &name : format_icons.getMemberNames()) {
      icons_map_.emplace(name, format_icons[name].asString());
    }

    icons_map_.emplace("", "");
  }

  auto config_all_outputs = config_["all-outputs"];
  if (config_all_outputs.isBool()) {
    all_outputs_ = config_all_outputs.asBool();
  }

  auto config_show_special = config_["show-special"];
  if (config_show_special.isBool()) {
    show_special_ = config_show_special.asBool();
  }

  auto config_active_only = config_["active-only"];
  if (config_active_only.isBool()) {
    active_only_ = config_active_only.asBool();
  }

  auto config_sort_by = config_["sort-by"];
  if (config_sort_by.isString()) {
    auto sort_by_str = config_sort_by.asString();
    try {
      sort_by_ = enum_parser_.sortStringToEnum(sort_by_str);
    } catch (const std::invalid_argument &e) {
      // Handle the case where the string is not a valid enum representation.
      sort_by_ = util::EnumParser::SORT_METHOD::DEFAULT;
      g_warning("Invalid string representation for sort-by. Falling back to default sort method.");
    }
  }
}

auto Workspaces::register_ipc() -> void {
  modulesReady = true;

  if (!gIPC) {
    gIPC = std::make_unique<IPC>();
  }

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
}

auto Workspaces::update() -> void {
  for (std::string workspace_to_remove : workspaces_to_remove_) {
    remove_workspace(workspace_to_remove);
  }

  workspaces_to_remove_.clear();

  for (Json::Value &workspace_to_create : workspaces_to_create_) {
    create_workspace(workspace_to_create);
  }

  workspaces_to_create_.clear();

  // get all active workspaces
  auto monitors = gIPC->getSocket1JsonReply("monitors");
  std::vector<std::string> visible_workspaces;
  for (Json::Value &monitor : monitors) {
    auto ws = monitor["activeWorkspace"];
    if (ws.isObject() && (ws["name"].isString())) {
      visible_workspaces.push_back(ws["name"].asString());
    }
  }

  for (auto &workspace : workspaces_) {
    // active
    workspace->set_active(workspace->name() == active_workspace_name_);
    // disable urgency if workspace is active
    if (workspace->name() == active_workspace_name_ && workspace->is_urgent()) {
      workspace->set_urgent(false);
    }

    // visible
    workspace->set_visible(std::find(visible_workspaces.begin(), visible_workspaces.end(),
                                     workspace->name()) != visible_workspaces.end());

    // set workspace icon
    std::string &workspace_icon = icons_map_[""];
    if (with_icon_) {
      workspace_icon = workspace->select_icon(icons_map_);
    }
    workspace->update(format_, workspace_icon);
  }
  AModule::update();
}

void Workspaces::onEvent(const std::string &ev) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string eventName(begin(ev), begin(ev) + ev.find_first_of('>'));
  std::string payload = ev.substr(eventName.size() + 2);

  if (eventName == "workspace") {
    active_workspace_name_ = payload;

  } else if (eventName == "destroyworkspace") {
    workspaces_to_remove_.push_back(payload);

  } else if (eventName == "createworkspace") {
    const Json::Value workspaces_json = gIPC->getSocket1JsonReply("workspaces");
    for (Json::Value workspace_json : workspaces_json) {
      std::string name = workspace_json["name"].asString();
      if (name == payload &&
          (all_outputs() || bar_.output->name == workspace_json["monitor"].asString()) &&
          (show_special() || !name.starts_with("special"))) {
        workspaces_to_create_.push_back(workspace_json);
        break;
      }
    }

  } else if (eventName == "focusedmon") {
    active_workspace_name_ = payload.substr(payload.find(',') + 1);

  } else if (eventName == "moveworkspace" && !all_outputs()) {
    std::string workspace = payload.substr(0, payload.find(','));
    std::string new_output = payload.substr(payload.find(',') + 1);
    if (bar_.output->name == new_output) {  // TODO: implement this better
      const Json::Value workspaces_json = gIPC->getSocket1JsonReply("workspaces");
      for (Json::Value workspace_json : workspaces_json) {
        std::string name = workspace_json["name"].asString();
        if (name == workspace && bar_.output->name == workspace_json["monitor"].asString()) {
          workspaces_to_create_.push_back(workspace_json);
          break;
        }
      }
    } else {
      workspaces_to_remove_.push_back(workspace);
    }
  } else if (eventName == "openwindow" || eventName == "closewindow" || eventName == "movewindow") {
    update_window_count();
  } else if (eventName == "urgent") {
    set_urgent_workspace(payload);
  } else if (eventName == "renameworkspace") {
    std::string workspace_id_str = payload.substr(0, payload.find(','));
    int workspace_id = workspace_id_str == "special" ? -99 : std::stoi(workspace_id_str);
    std::string new_name = payload.substr(payload.find(',') + 1);
    for (auto &workspace : workspaces_) {
      if (workspace->id() == workspace_id) {
        if (workspace->name() == active_workspace_name_) {
          active_workspace_name_ = new_name;
        }
        workspace->set_name(new_name);
        break;
      }
    }
  }

  dp.emit();
}

void Workspaces::update_window_count() {
  const Json::Value workspaces_json = gIPC->getSocket1JsonReply("workspaces");
  for (auto &workspace : workspaces_) {
    auto workspace_json = std::find_if(
        workspaces_json.begin(), workspaces_json.end(),
        [&](Json::Value const &x) { return x["name"].asString() == workspace->name(); });
    uint32_t count = 0;
    if (workspace_json != workspaces_json.end()) {
      try {
        count = (*workspace_json)["windows"].asUInt();
      } catch (const std::exception &e) {
        spdlog::error("Failed to update window count: {}", e.what());
      }
    }
    workspace->set_windows(count);
  }
}

void Workspaces::create_workspace(Json::Value &value) {
  // replace the existing persistent workspace if it exists
  auto workspace = std::find_if(
      workspaces_.begin(), workspaces_.end(), [&](std::unique_ptr<Workspace> const &x) {
        auto name = value["name"].asString();
        return x->is_persistent() &&
               ((name.starts_with("special:") && name.substr(8) == x->name()) || name == x->name());
      });
  if (workspace != workspaces_.end()) {
    // replace workspace, but keep persistent flag
    workspaces_.erase(workspace);
    value["persistent"] = true;
  }

  // create new workspace
  workspaces_.emplace_back(std::make_unique<Workspace>(value, *this));
  Gtk::Button &new_workspace_button = workspaces_.back()->button();
  box_.pack_start(new_workspace_button, false, false);
  sort_workspaces();
  new_workspace_button.show_all();
}

void Workspaces::remove_workspace(std::string name) {
  auto workspace = std::find_if(workspaces_.begin(), workspaces_.end(),
                                [&](std::unique_ptr<Workspace> &x) { return x->name() == name; });

  if (workspace == workspaces_.end()) {
    // happens when a workspace on another monitor is destroyed
    return;
  }

  if ((*workspace)->is_persistent()) {
    // don't remove persistent workspaces, create_workspace will take care of replacement
    return;
  }

  box_.remove(workspace->get()->button());
  workspaces_.erase(workspace);
}

void Workspaces::fill_persistent_workspaces() {
  if (config_["persistent_workspaces"].isObject()) {
    const Json::Value persistent_workspaces = config_["persistent_workspaces"];
    const std::vector<std::string> keys = persistent_workspaces.getMemberNames();

    for (const std::string &key : keys) {
      // only add if either:
      // 1. key is "*" and this monitor is not already defined in the config
      // 2. key is the current monitor name
      bool can_create =
          (key == "*" && std::find(keys.begin(), keys.end(), bar_.output->name) == keys.end()) ||
          key == bar_.output->name;
      const Json::Value &value = persistent_workspaces[key];

      if (value.isInt()) {
        // value is a number => create that many workspaces for this monitor
        if (can_create) {
          int amount = value.asInt();
          spdlog::debug("Creating {} persistent workspaces for monitor {}", amount,
                        bar_.output->name);
          for (int i = 0; i < amount; i++) {
            persistent_workspaces_to_create_.emplace_back(
                std::to_string(monitor_id_ * amount + i + 1));
          }
        }
      } else if (value.isArray() && !value.empty()) {
        // value is an array => create defined workspaces for this monitor
        if (can_create) {
          for (const Json::Value &workspace : value) {
            if (workspace.isInt()) {
              spdlog::debug("Creating workspace {} on monitor {}", workspace, bar_.output->name);
              persistent_workspaces_to_create_.emplace_back(std::to_string(workspace.asInt()));
            }
          }
        } else {
          // key is the workspace and value is array of monitors to create on
          for (const Json::Value &monitor : value) {
            if (monitor.isString() && monitor.asString() == bar_.output->name) {
              persistent_workspaces_to_create_.emplace_back(key);
              break;
            }
          }
        }
      } else {
        // this workspace should be displayed on all monitors
        persistent_workspaces_to_create_.emplace_back(key);
      }
    }
  }
}

void Workspaces::create_persistent_workspaces() {
  for (const std::string &workspace_name : persistent_workspaces_to_create_) {
    Json::Value new_workspace;
    try {
      // numbered persistent workspaces get the name as ID
      new_workspace["id"] = workspace_name == "special" ? -99 : std::stoi(workspace_name);
    } catch (const std::exception &e) {
      // named persistent workspaces start with ID=0
      new_workspace["id"] = 0;
    }
    new_workspace["name"] = workspace_name;
    new_workspace["monitor"] = bar_.output->name;
    new_workspace["windows"] = 0;
    new_workspace["persistent"] = true;

    create_workspace(new_workspace);
  }
}

void Workspaces::init() {
  active_workspace_name_ = (gIPC->getSocket1JsonReply("activeworkspace"))["name"].asString();

  // get monitor ID from name (used by persistent workspaces)
  monitor_id_ = 0;
  auto monitors = gIPC->getSocket1JsonReply("monitors");
  auto current_monitor = std::find_if(
      monitors.begin(), monitors.end(),
      [this](const Json::Value &m) { return m["name"].asString() == bar_.output->name; });
  if (current_monitor == monitors.end()) {
    spdlog::error("Monitor '{}' does not have an ID? Using 0", bar_.output->name);
  } else {
    monitor_id_ = (*current_monitor)["id"].asInt();
  }

  fill_persistent_workspaces();
  create_persistent_workspaces();

  const Json::Value workspaces_json = gIPC->getSocket1JsonReply("workspaces");
  for (Json::Value workspace_json : workspaces_json) {
    if ((all_outputs() || bar_.output->name == workspace_json["monitor"].asString()) &&
        (!workspace_json["name"].asString().starts_with("special") || show_special())) {
      create_workspace(workspace_json);
    }
  }

  update_window_count();

  sort_workspaces();

  dp.emit();
}

Workspaces::~Workspaces() {
  gIPC->unregisterForIPC(this);
  // wait for possible event handler to finish
  std::lock_guard<std::mutex> lg(mutex_);
}

Workspace::Workspace(const Json::Value &workspace_data, Workspaces &workspace_manager)
    : workspace_manager_(workspace_manager),
      id_(workspace_data["id"].asInt()),
      name_(workspace_data["name"].asString()),
      output_(workspace_data["monitor"].asString()),  // TODO:allow using monitor desc
      windows_(workspace_data["windows"].asInt()),
      active_(true) {
  if (name_.starts_with("name:")) {
    name_ = name_.substr(5);
  } else if (name_.starts_with("special")) {
    name_ = id_ == -99 ? name_ : name_.substr(8);
    is_special_ = true;
  }

  if (workspace_data.isMember("persistent")) {
    is_persistent_ = workspace_data["persistent"].asBool();
  }

  button_.add_events(Gdk::BUTTON_PRESS_MASK);
  button_.signal_button_press_event().connect(sigc::mem_fun(*this, &Workspace::handle_clicked),
                                              false);

  button_.set_relief(Gtk::RELIEF_NONE);
  content_.set_center_widget(label_);
  button_.add(content_);
}

void add_or_remove_class(const Glib::RefPtr<Gtk::StyleContext> &context, bool condition,
                         const std::string &class_name) {
  if (condition) {
    context->add_class(class_name);
  } else {
    context->remove_class(class_name);
  }
}

void Workspace::update(const std::string &format, const std::string &icon) {
  // clang-format off
  if (this->workspace_manager_.active_only() && \
     !this->active() && \
     !this->is_persistent() && \
     !this->is_visible() && \
     !this->is_special()) {
    // clang-format on
    // if active_only is true, hide if not active, persistent, visible or special
    button_.hide();
    return;
  }
  button_.show();

  auto style_context = button_.get_style_context();
  add_or_remove_class(style_context, active(), "active");
  add_or_remove_class(style_context, is_special(), "special");
  add_or_remove_class(style_context, is_empty(), "empty");
  add_or_remove_class(style_context, is_persistent(), "persistent");
  add_or_remove_class(style_context, is_urgent(), "urgent");
  add_or_remove_class(style_context, is_visible(), "visible");

  label_.set_markup(fmt::format(fmt::runtime(format), fmt::arg("id", id()),
                                fmt::arg("name", name()), fmt::arg("icon", icon)));
}

void Workspaces::sort_workspaces() {
  std::sort(workspaces_.begin(), workspaces_.end(),
            [&](std::unique_ptr<Workspace> &a, std::unique_ptr<Workspace> &b) {
              // Helper comparisons
              auto is_id_less = a->id() < b->id();
              auto is_name_less = a->name() < b->name();
              auto is_number_less = std::stoi(a->name()) < std::stoi(b->name());

              switch (sort_by_) {
                case util::EnumParser::SORT_METHOD::ID:
                  return is_id_less;
                case util::EnumParser::SORT_METHOD::NAME:
                  return is_name_less;
                case util::EnumParser::SORT_METHOD::NUMBER:
                  try {
                    return is_number_less;
                  } catch (const std::invalid_argument &) {
                    // Handle the exception if necessary.
                    break;
                  }
                case util::EnumParser::SORT_METHOD::DEFAULT:
                default:
                  // Handle the default case here.
                  // normal -> named persistent -> named -> special -> named special

                  // both normal (includes numbered persistent) => sort by ID
                  if (a->id() > 0 && b->id() > 0) {
                    return is_id_less;
                  }

                  // one normal, one special => normal first
                  if ((a->is_special()) ^ (b->is_special())) {
                    return b->is_special();
                  }

                  // only one normal, one named
                  if ((a->id() > 0) ^ (b->id() > 0)) {
                    return a->id() > 0;
                  }

                  // both special
                  if (a->is_special() && b->is_special()) {
                    // if one is -99 => put it last
                    if (a->id() == -99 || b->id() == -99) {
                      return b->id() == -99;
                    }
                    // both are 0 (not yet named persistents) / both are named specials (-98 <= ID
                    // <=-1)
                    return is_name_less;
                  }

                  // sort non-special named workspaces by name (ID <= -1377)
                  return is_name_less;
                  break;
              }

              // Return a default value if none of the cases match.
              return is_name_less;  // You can adjust this to your specific needs.
            });

  for (size_t i = 0; i < workspaces_.size(); ++i) {
    box_.reorder_child(workspaces_[i]->button(), i);
  }
}

std::string &Workspace::select_icon(std::map<std::string, std::string> &icons_map) {
  if (is_urgent()) {
    auto urgent_icon_it = icons_map.find("urgent");
    if (urgent_icon_it != icons_map.end()) {
      return urgent_icon_it->second;
    }
  }

  if (active()) {
    auto active_icon_it = icons_map.find("active");
    if (active_icon_it != icons_map.end()) {
      return active_icon_it->second;
    }
  }

  if (is_special()) {
    auto special_icon_it = icons_map.find("special");
    if (special_icon_it != icons_map.end()) {
      return special_icon_it->second;
    }
  }

  auto named_icon_it = icons_map.find(name());
  if (named_icon_it != icons_map.end()) {
    return named_icon_it->second;
  }

  if (is_visible()) {
    auto visible_icon_it = icons_map.find("visible");
    if (visible_icon_it != icons_map.end()) {
      return visible_icon_it->second;
    }
  }

  if (is_empty()) {
    auto empty_icon_it = icons_map.find("empty");
    if (empty_icon_it != icons_map.end()) {
      return empty_icon_it->second;
    }
  }

  if (is_persistent()) {
    auto persistent_icon_it = icons_map.find("persistent");
    if (persistent_icon_it != icons_map.end()) {
      return persistent_icon_it->second;
    }
  }

  auto default_icon_it = icons_map.find("default");
  if (default_icon_it != icons_map.end()) {
    return default_icon_it->second;
  }
  return icons_map[""];
}

auto Workspace::handle_clicked(GdkEventButton *bt) -> bool {
  try {
    if (id() > 0) {  // normal or numbered persistent
      gIPC->getSocket1Reply("dispatch workspace " + std::to_string(id()));
    } else if (!is_special()) {  // named
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
  return false;
}

void Workspaces::set_urgent_workspace(std::string windowaddress) {
  const Json::Value clients_json = gIPC->getSocket1JsonReply("clients");
  int workspace_id = -1;

  for (Json::Value client_json : clients_json) {
    if (client_json["address"].asString().ends_with(windowaddress)) {
      workspace_id = client_json["workspace"]["id"].asInt();
      break;
    }
  }

  auto workspace =
      std::find_if(workspaces_.begin(), workspaces_.end(),
                   [&](std::unique_ptr<Workspace> &x) { return x->id() == workspace_id; });
  if (workspace != workspaces_.end()) {
    workspace->get()->set_urgent();
  }
}

}  // namespace waybar::modules::hyprland
