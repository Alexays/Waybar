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

  box_.set_name("workspaces");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  event_box_.add(box_);
  modulesReady = true;
  if (!gIPC) {
    gIPC = std::make_unique<IPC>();
  }

  init();

  gIPC->registerForIPC("workspace", this);
  gIPC->registerForIPC("createworkspace", this);
  gIPC->registerForIPC("destroyworkspace", this);
  gIPC->registerForIPC("focusedmon", this);
  gIPC->registerForIPC("moveworkspace", this);
  gIPC->registerForIPC("openwindow", this);
  gIPC->registerForIPC("closewindow", this);
  gIPC->registerForIPC("movewindow", this);
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

  for (auto &workspace : workspaces_) {
    workspace->set_active(workspace->name() == active_workspace_name_);
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
      if (workspace_json["name"].asString() == payload &&
          (all_outputs() || bar_.output->name == workspace_json["monitor"].asString()) &&
          (show_special() || !workspace_json["name"].asString().starts_with("special"))) {
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
        if (workspace_json["name"].asString() == workspace &&
            bar_.output->name == workspace_json["monitor"].asString()) {
          workspaces_to_create_.push_back(workspace_json);
          break;
        }
      }
    } else {
      workspaces_to_remove_.push_back(workspace);
    }
  } else if (eventName == "openwindow" || eventName == "closewindow" || eventName == "movewindow") {
    update_window_count();
  }

  dp.emit();
}

void Workspaces::update_window_count() {
  const Json::Value workspaces_json = gIPC->getSocket1JsonReply("workspaces");
  for (auto &workspace : workspaces_) {
    auto workspace_json = std::find_if(
        workspaces_json.begin(), workspaces_json.end(),
        [&](Json::Value const &x) { return x["name"].asString() == workspace->name(); });
    if (workspace_json != workspaces_json.end()) {
      try {
        workspace->set_windows((*workspace_json)["windows"].asUInt());
      } catch (const std::exception &e) {
        spdlog::error("Failed to update window count: {}", e.what());
      }
    } else {
      workspace->set_windows(0);
    }
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
  workspaces_.emplace_back(std::make_unique<Workspace>(value));
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
  if (config_["persistent_workspaces"].isObject() && !all_outputs()) {
    const Json::Value persistent_workspaces = config_["persistent_workspaces"];
    const std::vector<std::string> keys = persistent_workspaces.getMemberNames();

    for (const std::string &key : keys) {
      const Json::Value &value = persistent_workspaces[key];
      if (value.isInt()) {
        // value is a number => create that many workspaces for this monitor
        // only add if either:
        // 1. key is "*" and this monitor is not already defined in the config
        // 2. key is the current monitor name
        if ((key == "*" && std::find(keys.begin(), keys.end(), bar_.output->name) == keys.end()) ||
            key == bar_.output->name) {
          int amount = value.asInt();
          spdlog::debug("Creating {} persistent workspaces for monitor {}", amount,
                        bar_.output->name);
          for (int i = 0; i < amount; i++) {
            persistent_workspaces_to_create_.emplace_back(
                std::to_string(monitor_id_ * amount + i + 1));
          }
        }

      } else if (value.isArray() && !value.empty()) {
        // value is an array => key is a workspace name
        // values are monitor names this workspace should be shown on
        for (const Json::Value &monitor : value) {
          if (monitor.isString() && monitor.asString() == bar_.output->name) {
            persistent_workspaces_to_create_.emplace_back(key);
            break;
          }
        }
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
        (!workspace_json["name"].asString().starts_with("special") || show_special()))
      create_workspace(workspace_json);
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

Workspace::Workspace(const Json::Value &workspace_data)
    : id_(workspace_data["id"].asInt()),
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
};

void add_or_remove_class(const Glib::RefPtr<Gtk::StyleContext> &context, bool condition,
                         const std::string &class_name) {
  if (condition) {
    context->add_class(class_name);
  } else {
    context->remove_class(class_name);
  }
}

void Workspace::update(const std::string &format, const std::string &icon) {
  auto style_context = button_.get_style_context();
  add_or_remove_class(style_context, active(), "active");
  add_or_remove_class(style_context, is_special(), "special");
  add_or_remove_class(style_context, is_empty(), "persistent");

  label_.set_markup(fmt::format(fmt::runtime(format), fmt::arg("id", id()),
                                fmt::arg("name", name()), fmt::arg("icon", icon)));
}

void Workspaces::sort_workspaces() {
  std::sort(workspaces_.begin(), workspaces_.end(),
            [](std::unique_ptr<Workspace> &a, std::unique_ptr<Workspace> &b) {
              // normal -> named persistent -> named -> special -> named special

              // both normal (includes numbered persistent) => sort by ID
              if (a->id() > 0 && b->id() > 0) {
                return a->id() < b->id();
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
                // both are 0 (not yet named persistents) / both are named specials (-98 <= ID <=-1)
                return a->name() < b->name();
              }

              // sort non-special named workspaces by name (ID <= -1377)
              return a->name() < b->name();
            });

  for (size_t i = 0; i < workspaces_.size(); ++i) {
    box_.reorder_child(workspaces_[i]->button(), i);
  }
}

std::string &Workspace::select_icon(std::map<std::string, std::string> &icons_map) {
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

}  // namespace waybar::modules::hyprland
