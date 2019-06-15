#include "modules/sway/taskbar.hpp"

namespace waybar::modules::sway {

TaskBar::TaskBar(const std::string& id, const Bar& bar, const Json::Value& config)
    : bar_(bar),
      config_(config),
      box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0),
      scrolling_(false) {
  box_.set_name("taskbar");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  ipc_.subscribe(R"(["workspace", "window"])");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &TaskBar::onEvent));
  ipc_.signal_cmd.connect(sigc::mem_fun(*this, &TaskBar::onCmd));
  ipc_.sendCmd(IPC_GET_TREE);
  if (!config["disable-bar-scroll"].asBool()) {
    auto& window = const_cast<Bar&>(bar_).window;
    window.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
    window.signal_scroll_event().connect(sigc::mem_fun(*this, &TaskBar::handleScroll));
  }
  dp.emit();
  worker();
}

void TaskBar::onEvent(const struct Ipc::ipc_response& res) {
  try {
    std::lock_guard<std::mutex> lock(mutex_);

    if (static_cast<int>(res.type) == IPC_EVENT_WORKSPACE) {
      auto payload = parser_.parse(res.payload);
      if (taskMap_.empty()) {
        ipc_.sendCmd(IPC_GET_TREE);
      }
      ipc_.sendCmd(IPC_GET_WORKSPACES);
    } else if (static_cast<int>(res.type) == IPC_EVENT_WINDOW) {
      ipc_.sendCmd(IPC_GET_TREE);
    }

    dp.emit();
  } catch (const std::exception& e) {
    std::cerr << "TaskBar: " << e.what() << std::endl;
  }
}

void TaskBar::onCmd(const struct Ipc::ipc_response& res) {
  try {
    auto payload = parser_.parse(res.payload);
    if (res.type == IPC_GET_TREE) {
      taskMap_.clear();
      parseTree(payload);
    }
    if (res.type == IPC_GET_WORKSPACES) {
      parseCurrentWorkspace(payload);
    }

  } catch (const std::exception& e) {
    std::cerr << "TaskBar: " << e.what() << std::endl;
  }
}

void TaskBar::worker() {
  thread_ = [this] {
    try {
      ipc_.handleEvent();
    } catch (const std::exception& e) {
      std::cerr << "TaskBar: " << e.what() << std::endl;
    }
  };
}
bool TaskBar::filterButtons() {
  bool needReorder = false;
  for (auto it = buttons_.begin(); it != buttons_.end();) {
    auto ws = std::find_if(taskMap_[current_workspace].applications.begin(),
                           taskMap_[current_workspace].applications.end(),
                           [it](const auto& value) { return value.id == it->first; });
    if (ws == taskMap_[current_workspace].applications.end()) {
      it = buttons_.erase(it);
      needReorder = true;
    } else {
      ++it;
    }
  }
  return needReorder;
}

auto TaskBar::update() -> void {
  std::lock_guard<std::mutex> lock(mutex_);
  bool                        needReorder = filterButtons();
  if (current_workspace == "") current_workspace = taskMap_.begin()->first;
  for (auto it = taskMap_[current_workspace].applications.begin();
       it != taskMap_[current_workspace].applications.end();
       ++it) {
    auto bit = buttons_.find(it->id);
    if (bit == buttons_.end()) {
      needReorder = true;
    }
    auto& button = bit == buttons_.end() ? addButton(*it) : bit->second;
    if (it - taskMap_[current_workspace].applications.begin() ==
        taskMap_[current_workspace].focused_num) {
      button.get_style_context()->add_class("focused");
    } else {
      button.get_style_context()->remove_class("focused");
    }
    if (needReorder) {
      box_.reorder_child(button, it - taskMap_[current_workspace].applications.begin());
    }
    std::string output = "";
    if (config_["format"].isString()) {
      auto format = config_["format"].asString();
      output = fmt::format(format, fmt::arg("name", it->display_name), fmt::arg("icon", ""));
    }
    if (!config_["disable-markup"].asBool()) {
      if (!button.get_image()) {
        static_cast<Gtk::Label*>(button.get_child())->set_markup(output);
      } else {
        auto button_inner_container = dynamic_cast<Gtk::Container*>(button.get_child());
        auto label_and_icon_container =
            dynamic_cast<Gtk::Container*>(button_inner_container->get_children()[0]);

        // Here 1 is index of Gtk::Label, while 0 is index of Gtk::Image
        // It's undocumented, but behaviour is pretty stable during significant amount of testing
        auto label_of_button =
            dynamic_cast<Gtk::Label*>(label_and_icon_container->get_children()[1]);
        label_of_button->set_markup(output);
      }

    } else {
      button.set_label(output);
    }

    onButtonReady(button);
  }
}

Gtk::Button& TaskBar::addButton(const Application& application) {
  auto  pair = buttons_.emplace(application.id, application.display_name);
  auto& button = pair.first->second;
  if (config_["format"].asString().find("{icon}") != std::string::npos) {
    button.set_image_from_icon_name(application.instante_name);
  }
  box_.pack_start(button, false, false, 0);
  button.set_relief(Gtk::RELIEF_NONE);
  button.signal_clicked().connect([this, pair] {
    try {
      ipc_.sendCmd(IPC_COMMAND,
                   fmt::format("[con_id={}] focus", std::to_string(pair.first->first)));
    } catch (const std::exception& e) {
      spdlog::error("TaskBar: {}", e.what());
    }
  });
  if (!config_["disable-scroll"].asBool()) {
    button.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
    button.signal_scroll_event().connect(sigc::mem_fun(*this, &TaskBar::handleScroll));
  }
  return button;
}

bool TaskBar::handleScroll(GdkEventScroll* e) {
  // Avoid concurrent scroll event
  if (scrolling_) {
    return false;
  }
  int new_focus = 0;
  scrolling_ = true;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    int                         current_focus = taskMap_[current_workspace].focused_num;

    switch (e->direction) {
      case GDK_SCROLL_DOWN:
      case GDK_SCROLL_RIGHT:
        new_focus = getCycleTasks(current_focus, false);
        break;
      case GDK_SCROLL_UP:
      case GDK_SCROLL_LEFT:
        new_focus = getCycleTasks(current_focus, true);
        break;
      case GDK_SCROLL_SMOOTH:
        gdouble delta_x, delta_y;
        gdk_event_get_scroll_deltas(reinterpret_cast<const GdkEvent*>(e), &delta_x, &delta_y);
        if (delta_y < 0) {
          new_focus = getCycleTasks(current_focus, true);
        } else if (delta_y > 0) {
          new_focus = getCycleTasks(current_focus, false);
        }
        break;
      default:
        break;
    }
    if (new_focus == current_focus) {
      scrolling_ = false;
      return false;
    }
    taskMap_[current_workspace].focused_num = new_focus;
  }
  try {
    ipc_.sendCmd(
        IPC_COMMAND,
        fmt::format("[con_id={}] focus", taskMap_[current_workspace].applications[new_focus].id));
    scrolling_ = false;
  } catch (const std::exception& e) {
    std::cerr << "TaskBar: " << e.what() << std::endl;
  }
  return true;
}
const int TaskBar::getCycleTasks(int current_focus, bool prev) {
  int application_max_index = taskMap_[current_workspace].applications.size() - 1;
  if (prev && !current_focus) {
    return application_max_index;
  } else if (prev && current_focus)
    return --current_focus;
  else if (!prev && current_focus != application_max_index)
    return ++current_focus;
  else
    return 0;
}
void     TaskBar::onButtonReady(Gtk::Button& button) { button.show(); }
TaskBar::operator Gtk::Widget&() { return box_; }

void TaskBar::parseTree(const Json::Value& nodes) {
  for (auto const& node : nodes["nodes"]) {
    if (node["type"] == "workspace") {
      for (auto const& window : node["nodes"]) {
        if (window["type"] == "con") {
          std::string workspace_name = node["name"].asString();
          if (!taskMap_.count(workspace_name)) {
            WorkspaceMap wp;
            taskMap_.emplace(workspace_name, wp);
          }
          size_t max_length = config_["max_length"].isUInt() ? config_["max_length"].asUInt() : 0;
          std::string window_name = max_length && window["name"].asString().length() > max_length
                                        ? std::string(window["name"].asString(), 0, max_length)
                                        : window["name"].asString();

          std::string instance_name = "terminal";
          if (!window["window_properties"].isNull()) {
            instance_name = window["window_properties"]["class"].asString();
            std::transform(
                instance_name.begin(), instance_name.end(), instance_name.begin(), ::tolower);
            if (instance_name == "pcmanfm") {
              instance_name = "system-file-manager";
            }
          }
          taskMap_[workspace_name].applications.push_back(
              {window["id"].asInt(), window_name, instance_name});
          if (window["focused"].asBool())
            taskMap_[workspace_name].focused_num =
                taskMap_.at(workspace_name).applications.size() - 1;
        }
      }
    }
    parseTree(node);
  }
}

void TaskBar::parseCurrentWorkspace(const Json::Value& nodes) {
  for (auto const& node : nodes) {
    if (node["focused"].isBool() && node["focused"].asBool())
      current_workspace = node["name"].asString();
  }
}
}  // namespace waybar::modules::sway
