#include "modules/sway/workspaces.hpp"
#include "modules/sway/ipc/client.hpp"

waybar::modules::sway::Workspaces::Workspaces(Bar &bar, Json::Value config)
  : bar_(bar), config_(std::move(config)), scrolling_(false)
{
  box_.set_name("workspaces");
  std::string socketPath = getSocketPath();
  ipcfd_ = ipcOpenSocket(socketPath);
  ipc_eventfd_ = ipcOpenSocket(socketPath);
  ipcSingleCommand(ipc_eventfd_, IPC_SUBSCRIBE, "[ \"workspace\" ]");
  thread_.sig_update.connect(sigc::mem_fun(*this, &Workspaces::update));
  thread_ = [this] {
    try {
      // Wait for the name of the output
      if (!config_["all-outputs"].asBool() && bar_.output_name.empty()) {
        while (bar_.output_name.empty()) {
          thread_.sleep_for(chrono::milliseconds(150));
        }
      } else if (!workspaces_.empty()) {
        ipcRecvResponse(ipc_eventfd_);
      }
      std::lock_guard<std::mutex> lock(mutex_);
      auto res = ipcSingleCommand(ipcfd_, IPC_GET_WORKSPACES, "");
      workspaces_ = parser_.parse(res.payload);
      thread_.emit();
    } catch (const std::exception& e) {
      std::cerr << e.what() << std::endl;
    }
  };
}

waybar::modules::sway::Workspaces::~Workspaces()
{
  close(ipcfd_);
  close(ipc_eventfd_);
}

auto waybar::modules::sway::Workspaces::update() -> void
{
  std::lock_guard<std::mutex> lock(mutex_);
  bool needReorder = false;
  for (auto it = buttons_.begin(); it != buttons_.end();) {
    auto ws = std::find_if(workspaces_.begin(), workspaces_.end(),
      [it](auto node) -> bool { return node["num"].asInt() == it->first; });
    if (ws == workspaces_.end()) {
      it = buttons_.erase(it);
      needReorder = true;
    } else {
      ++it;
    }
  }
  for (auto node : workspaces_) {
    if (!config_["all-outputs"].asBool()
      && bar_.output_name != node["output"].asString()) {
      continue;
    }
    auto it = buttons_.find(node["num"].asInt());
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
        box_.reorder_child(button, node["num"].asInt());
      }
      button.show();
    }
  }
  if (scrolling_) {
    scrolling_ = false;
  }
}

void waybar::modules::sway::Workspaces::addWorkspace(Json::Value node)
{
  auto icon = getIcon(node["name"].asString());
  auto pair = buttons_.emplace(node["num"].asInt(), icon);
  auto &button = pair.first->second;
  if (icon != node["name"].asString()) {
    button.get_style_context()->add_class("icon");
  }
  box_.pack_start(button, false, false, 0);
  button.set_relief(Gtk::RELIEF_NONE);
  button.signal_clicked().connect([this, pair] {
    try {
      std::lock_guard<std::mutex> lock(mutex_);
      auto cmd = fmt::format("workspace \"{}\"", pair.first->first);
      ipcSingleCommand(ipcfd_, IPC_COMMAND, cmd);
    } catch (const std::exception& e) {
      std::cerr << e.what() << std::endl;
    }
  });
  button.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
  if (!config_["disable-scroll"].asBool()) {
    button.signal_scroll_event()
      .connect(sigc::mem_fun(*this, &Workspaces::handleScroll));
  }
  box_.reorder_child(button, node["num"].asInt());
  if (node["focused"].asBool()) {
    button.get_style_context()->add_class("focused");
  }
  if (node["visible"].asBool()) {
    button.get_style_context()->add_class("visible");
  }
  if (node["urgent"].asBool()) {
    button.get_style_context()->add_class("urgent");
  }
  button.show();
}

std::string waybar::modules::sway::Workspaces::getIcon(std::string name)
{
  if (config_["format-icons"][name]) {
    return config_["format-icons"][name].asString();
  }
  if (config_["format-icons"]["default"]) {
    return config_["format-icons"]["default"].asString();
  }
  return name;
}

bool waybar::modules::sway::Workspaces::handleScroll(GdkEventScroll *e)
{
  // Avoid concurrent scroll event
  if (scrolling_) {
    return false;
  }
  scrolling_ = true;
  int id = -1;
  uint16_t idx = 0;
  std::lock_guard<std::mutex> lock(mutex_);
  for (; idx < workspaces_.size(); idx += 1) {
    if (workspaces_[idx]["focused"].asBool()) {
      id = workspaces_[idx]["num"].asInt();
      break;
    }
  }
  if (id == -1) {
    scrolling_ = false;
    return false;
  }
  if (e->direction == GDK_SCROLL_UP) {
      id = getNextWorkspace();
  }
  if (e->direction == GDK_SCROLL_DOWN) {
      id = getPrevWorkspace();
  }
  if (e->direction == GDK_SCROLL_SMOOTH) {
    gdouble delta_x, delta_y;
    gdk_event_get_scroll_deltas(reinterpret_cast<const GdkEvent *>(e),
      &delta_x, &delta_y);
    if (delta_y < 0) {
      id = getNextWorkspace();
    } else if (delta_y > 0) {
      id = getPrevWorkspace();
    }
  }
  if (id == workspaces_[idx]["num"].asInt()) {
    scrolling_ = false;
    return false;
  }
  ipcSingleCommand(ipcfd_, IPC_COMMAND, fmt::format("workspace \"{}\"", id));
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  return true;
}

int waybar::modules::sway::Workspaces::getPrevWorkspace()
{
  for (uint16_t i = 0; i != workspaces_.size(); i += 1) {
    if (workspaces_[i]["focused"].asBool()) {
      if (i > 0) {
        return workspaces_[i - 1]["num"].asInt();
      }
      return workspaces_[workspaces_.size() - 1]["num"].asInt();
    }
  }
  return -1;
}

int waybar::modules::sway::Workspaces::getNextWorkspace()
{
  for (uint16_t i = 0; i != workspaces_.size(); i += 1) {
    if (workspaces_[i]["focused"].asBool()) {
      if (i + 1U < workspaces_.size()) {
        return workspaces_[i + 1]["num"].asInt();
      }
      return workspaces_[0]["num"].asInt();
    }
  }
  return -1;
}

waybar::modules::sway::Workspaces::operator Gtk::Widget &() {
  return box_;
}
