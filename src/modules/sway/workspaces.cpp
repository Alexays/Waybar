#include "modules/sway/workspaces.hpp"

namespace waybar::modules::sway {

Workspaces::Workspaces(const std::string &id, const Bar &bar, const Json::Value &config)
    : bar_(bar),
      config_(config),
      box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0),
      scrolling_(false) {
  box_.set_name("workspaces");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  ipc_.subscribe("[ \"workspace\" ]");
  ipc_.signal_cmd.connect(sigc::mem_fun(*this, &Workspaces::onCmd));
  // Launch worker
  worker();
}

void Workspaces::onCmd(const struct Ipc::ipc_response res) {
  if (thread_.isRunning()) {
    std::lock_guard<std::mutex> lock(mutex_);
    workspaces_ = parser_.parse(res.payload);
    dp.emit();
  }
}

const Json::Value Workspaces::getWorkspaces() {
  std::lock_guard<std::mutex> lock(mutex_);
  return workspaces_;
}

void Workspaces::worker() {
  thread_ = [this] {
    try {
      if (!getWorkspaces().empty()) {
        ipc_.handleEvent();
      }
      if (thread_.isRunning()) {
        ipc_.sendCmd(IPC_GET_WORKSPACES);
      }
    } catch (const std::exception &e) {
      std::cerr << "Workspaces: " << e.what() << std::endl;
    }
  };
}

auto Workspaces::update() -> void {
  bool needReorder = false;
  auto workspaces = getWorkspaces();
  for (auto it = buttons_.begin(); it != buttons_.end();) {
    auto ws = std::find_if(workspaces.begin(), workspaces.end(), [it](auto node) -> bool {
      return node["name"].asString() == it->first;
    });
    if (ws == workspaces.end() ||
        (!config_["all-outputs"].asBool() && (*ws)["output"].asString() != bar_.output->name)) {
      it = buttons_.erase(it);
      needReorder = true;
    } else {
      ++it;
    }
  }
  for (auto const &node : workspaces) {
    if (!config_["all-outputs"].asBool() && bar_.output->name != node["output"].asString()) {
      continue;
    }
    auto it = buttons_.find(node["name"].asString());
    if (it == buttons_.end()) {
      addWorkspace(node);
      needReorder = true;
    } else {
      auto &button = it->second;
      if (node["focused"].asBool()) {
        button.get_style_context()->add_class("focused");
      } else {
        button.get_style_context()->remove_class("focused");
      }
      if (node["visible"].asBool()) {
        button.get_style_context()->add_class("visible");
      } else {
        button.get_style_context()->remove_class("visible");
      }
      if (node["urgent"].asBool()) {
        button.get_style_context()->add_class("urgent");
      } else {
        button.get_style_context()->remove_class("urgent");
      }
      if (needReorder) {
        box_.reorder_child(button, getWorkspaceIndex(workspaces, node["name"].asString()));
      }
      auto        icon = getIcon(node["name"].asString(), node);
      std::string output = icon;
      if (config_["format"].isString()) {
        auto format = config_["format"].asString();
        output = fmt::format(format,
                             fmt::arg("icon", icon),
                             fmt::arg("name", trimWorkspaceName(node["name"].asString())),
                             fmt::arg("index", node["num"].asString()));
      }
      if (!config_["disable-markup"].asBool()) {
        static_cast<Gtk::Label *>(button.get_children()[0])->set_markup(output);
      } else {
        button.set_label(output);
      }
      onButtonReady(node, button);
    }
  }
  if (scrolling_) {
    scrolling_ = false;
  }
}

void Workspaces::addWorkspace(const Json::Value &node) {
  auto icon = getIcon(node["name"].asString(), node);
  auto format = config_["format"].isString()
                    ? fmt::format(config_["format"].asString(),
                                  fmt::arg("icon", icon),
                                  fmt::arg("name", trimWorkspaceName(node["name"].asString())),
                                  fmt::arg("index", node["num"].asString()))
                    : icon;
  auto  pair = buttons_.emplace(node["name"].asString(), format);
  auto &button = pair.first->second;
  if (!config_["disable-markup"].asBool()) {
    static_cast<Gtk::Label *>(button.get_children()[0])->set_markup(format);
  }
  box_.pack_start(button, false, false, 0);
  button.set_relief(Gtk::RELIEF_NONE);
  button.signal_clicked().connect([this, pair] {
    try {
      auto cmd = fmt::format("workspace \"{}\"", pair.first->first);
      ipc_.sendCmd(IPC_COMMAND, cmd);
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
    }
  });
  if (!config_["disable-scroll"].asBool()) {
    button.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
    button.signal_scroll_event().connect(sigc::mem_fun(*this, &Workspaces::handleScroll));
  }
  auto workspaces = getWorkspaces();
  box_.reorder_child(button, getWorkspaceIndex(workspaces, node["name"].asString()));
  if (node["focused"].asBool()) {
    button.get_style_context()->add_class("focused");
  }
  if (node["visible"].asBool()) {
    button.get_style_context()->add_class("visible");
  }
  if (node["urgent"].asBool()) {
    button.get_style_context()->add_class("urgent");
  }

  onButtonReady(node, button);
}

std::string Workspaces::getIcon(const std::string &name, const Json::Value &node) {
  std::vector<std::string> keys = {name, "urgent", "focused", "visible", "default"};
  for (auto const &key : keys) {
    if (key == "focused" || key == "visible" || key == "urgent") {
      if (config_["format-icons"][key].isString() && node[key].asBool()) {
        return config_["format-icons"][key].asString();
      }
    } else if (config_["format-icons"][key].isString()) {
      return config_["format-icons"][key].asString();
    }
  }
  return name;
}

bool Workspaces::handleScroll(GdkEventScroll *e) {
  // Avoid concurrent scroll event
  if (scrolling_) {
    return false;
  }
  auto    workspaces = getWorkspaces();
  uint8_t idx;
  scrolling_ = true;
  for (idx = 0; idx < workspaces.size(); idx += 1) {
    if (workspaces[idx]["focused"].asBool()) {
      break;
    }
  }
  if (idx == workspaces.size()) {
    scrolling_ = false;
    return false;
  }
  std::string name;
  if (e->direction == GDK_SCROLL_UP) {
    name = getCycleWorkspace(workspaces, idx, true);
  }
  if (e->direction == GDK_SCROLL_DOWN) {
    name = getCycleWorkspace(workspaces, idx, false);
  }
  if (e->direction == GDK_SCROLL_SMOOTH) {
    gdouble delta_x, delta_y;
    gdk_event_get_scroll_deltas(reinterpret_cast<const GdkEvent *>(e), &delta_x, &delta_y);
    if (delta_y < 0) {
      name = getCycleWorkspace(workspaces, idx, true);
    } else if (delta_y > 0) {
      name = getCycleWorkspace(workspaces, idx, false);
    }
  }
  if (name.empty() || name == workspaces[idx]["name"].asString()) {
    scrolling_ = false;
    return false;
  }
  ipc_.sendCmd(IPC_COMMAND, fmt::format("workspace \"{}\"", name));
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  return true;
}

const std::string Workspaces::getCycleWorkspace(const Json::Value &workspaces,
                                                uint8_t focused_workspace, bool prev) const {
  auto    inc = prev ? -1 : 1;
  int     size = workspaces.size();
  uint8_t idx = 0;
  for (int i = focused_workspace; i < size && i >= 0; i += inc) {
    bool same_output = (workspaces[i]["output"].asString() == bar_.output->name &&
                        !config_["all-outputs"].asBool()) ||
                       config_["all-outputs"].asBool();
    bool same_name =
        workspaces[i]["name"].asString() == workspaces[focused_workspace]["name"].asString();
    if (same_output && !same_name) {
      return workspaces[i]["name"].asString();
    }
    if (prev && i - 1 < 0) {
      i = size;
    } else if (!prev && i + 1 >= size) {
      i = -1;
    } else if (idx >= workspaces.size()) {
      return "";
    }
    idx += 1;
  }
  return "";
}

uint16_t Workspaces::getWorkspaceIndex(const Json::Value &workspaces,
                                       const std::string &name) const {
  uint16_t idx = 0;
  for (const auto &workspace : workspaces) {
    if (workspace["name"].asString() == name) {
      return idx;
    }
    if (!(!config_["all-outputs"].asBool() &&
          workspace["output"].asString() != bar_.output->name)) {
      idx += 1;
    }
  }
  return workspaces.size();
}

std::string Workspaces::trimWorkspaceName(std::string name) {
  std::size_t found = name.find(":");
  if (found != std::string::npos) {
    return name.substr(found + 1);
  }
  return name;
}

void Workspaces::onButtonReady(const Json::Value &node, Gtk::Button &button) {
  if (config_["current-only"].asBool()) {
    if (node["focused"].asBool()) {
      button.show();
    } else {
      button.hide();
    }
  } else {
    button.show();
  }
}

Workspaces::operator Gtk::Widget &() { return box_; }

}  // namespace waybar::modules::sway