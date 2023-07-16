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

  format_ = config_format.isString() ? config_format.asString() : "{id}";
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
  if (!gIPC.get()) {
    gIPC = std::make_unique<IPC>();
  }

  init();

  gIPC->registerForIPC("workspace", this);
  gIPC->registerForIPC("createworkspace", this);
  gIPC->registerForIPC("destroyworkspace", this);
  gIPC->registerForIPC("focusedmon", this);
  gIPC->registerForIPC("moveworkspace", this);
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
    workspace->set_active(workspace->name() == active_workspace_name);
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
    active_workspace_name = payload;

  } else if (eventName == "destroyworkspace") {
    workspaces_to_remove_.push_back(payload);

  } else if (eventName == "createworkspace") {
    const Json::Value workspaces_json = gIPC->getSocket1JsonReply("workspaces");
    for (Json::Value workspace_json : workspaces_json) {
      if (workspace_json["name"].asString() == payload &&
          (all_outputs() || bar_.output->name == workspace_json["monitor"].asString()) &&
          (workspace_json["name"].asString().find("special:") != 0 || show_special())) {
        workspaces_to_create_.push_back(workspace_json);
        break;
      }
    }

  } else if (eventName == "focusedmon") {
    active_workspace_name = payload.substr(payload.find(",") + 1);

  } else if (eventName == "moveworkspace" && !all_outputs()) {
    std::string workspace = payload.substr(0, payload.find(","));
    std::string new_output = payload.substr(payload.find(",") + 1);
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
  }

  dp.emit();
}

void Workspaces::create_workspace(Json::Value &value) {
  workspaces_.push_back(std::make_unique<Workspace>(value));
  Gtk::Button &new_workspace_button = workspaces_.back()->button();
  box_.pack_start(new_workspace_button, false, false);
  sort_workspaces();
  new_workspace_button.show_all();
}

void Workspaces::remove_workspace(std::string name) {
  auto workspace = std::find_if(workspaces_.begin(), workspaces_.end(),
                                [&](std::unique_ptr<Workspace> &x) { return x->name() == name; });

  if (workspace == workspaces_.end()) {
    return;
  }

  box_.remove(workspace->get()->button());
  workspaces_.erase(workspace);
}

void Workspaces::init() {
  active_workspace_name = (gIPC->getSocket1JsonReply("activeworkspace"))["name"].asString();

  const Json::Value workspaces_json = gIPC->getSocket1JsonReply("workspaces");
  for (Json::Value workspace_json : workspaces_json) {
    if ((all_outputs() || bar_.output->name == workspace_json["monitor"].asString()) &&
        (workspace_json["name"].asString().find("special") != 0 || show_special()))
      create_workspace(workspace_json);
  }

  sort_workspaces();

  dp.emit();
}

Workspaces::~Workspaces() {
  gIPC->unregisterForIPC(this);
  // wait for possible event handler to finish
  std::lock_guard<std::mutex> lg(mutex_);
}

Workspace::Workspace(const Json::Value &value)
    : id_(value["id"].asInt()),
      name_(value["name"].asString()),
      output_(value["monitor"].asString()),  // TODO:allow using monitor desc
      windows_(value["id"].asInt()) {
  active_ = true;
  is_special_ = false;

  if (name_.find("name:") == 0) {
    name_ = name_.substr(5);
  } else if (name_.find("special") == 0) {
    name_ = id_ == -99 ? name_ : name_.substr(13);
    is_special_ = 1;
  }

  button_.add_events(Gdk::BUTTON_PRESS_MASK);
  button_.signal_button_press_event().connect(sigc::mem_fun(*this, &Workspace::handle_clicked),
                                              false);

  button_.set_relief(Gtk::RELIEF_NONE);
  content_.set_center_widget(label_);
  button_.add(content_);
};

void add_or_remove_class(Glib::RefPtr<Gtk::StyleContext> context, bool condition,
                         const std::string &class_name) {
  if (condition) {
    context->add_class(class_name);
  } else {
    context->remove_class(class_name);
  }
}

void Workspace::update(const std::string &format, const std::string &icon) {
  Glib::RefPtr<Gtk::StyleContext> style_context = button_.get_style_context();
  add_or_remove_class(style_context, active(), "active");

  label_.set_markup(fmt::format(fmt::runtime(format), fmt::arg("id", id()),
                                fmt::arg("name", name()), fmt::arg("icon", icon)));
}

void Workspaces::sort_workspaces() {
  std::sort(workspaces_.begin(), workspaces_.end(),
            [](std::unique_ptr<Workspace> &a, std::unique_ptr<Workspace> &b) {
              // normal -> named -> special -> named special
              if (a->id() > 0 && b->id() > 0) {
                return a->id() < b->id();
              }
              if (a->id() < 0 && b->id() < 0) {
                if ((a->is_special()) ^ (a->is_special())) {
                  return a->id() > b->id();
                } else {
                  return a->id() < b->id();
                }
              }
              if ((a->id() > 0) ^ (b->id() > 0)) {
                return a->id() > b->id();
              }
              spdlog::error("huh!!?");
              return false;
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

  auto named_icon_it = icons_map.find(std::to_string(id()));
  if (named_icon_it != icons_map.end()) {
    return named_icon_it->second;
  }

  auto default_icon_it = icons_map.find("default");
  if (default_icon_it != icons_map.end()) {
    return default_icon_it->second;
  }
  return icons_map[""];
}

auto Workspace::handle_clicked(GdkEventButton *bt) -> bool {
  try {
    if (id() > 0) {  // normal
      gIPC->getSocket1Reply("dispatch workspace " + std::to_string(id()));
    } else if (!is_special()) {  // named normal
      gIPC->getSocket1Reply("dispatch workspace name" + name());
    } else if (id() != -99) {  // named special
      gIPC->getSocket1Reply("dispatch togglespecialworkspace name" + name());
    } else {  // special
      gIPC->getSocket1Reply("dispatch togglespecialworkspace special");
    }
    return true;
  } catch (const std::exception &e) {
    spdlog::error("Failed to dispatch workspace: {}", e.what());
  }
  return false;
}

}  // namespace waybar::modules::hyprland
