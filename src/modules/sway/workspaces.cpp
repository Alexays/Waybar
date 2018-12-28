#include "modules/sway/workspaces.hpp"

waybar::modules::sway::Workspaces::Workspaces(const std::string& id, const Bar& bar,
  const Json::Value& config)
  : bar_(bar), config_(config), scrolling_(false)
{
  box_.set_name("workspaces");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  ipc_.connect();
  ipc_.subscribe("[ \"workspace\" ]");
  // Launch worker
  worker();
}

void waybar::modules::sway::Workspaces::worker()
{
  thread_ = [this] {
    try {
      // Wait for the name of the output
      if (!config_["all-outputs"].asBool() && bar_.output_name.empty()) {
        while (bar_.output_name.empty()) {
          thread_.sleep_for(chrono::milliseconds(150));
        }
      } else if (thread_.isRunning() && !workspaces_.empty()) {
        ipc_.handleEvent();
      }
      {
        std::lock_guard<std::mutex> lock(mutex_);
        auto res = ipc_.sendCmd(IPC_GET_WORKSPACES);
        workspaces_ = parser_.parse(res.payload);
      }
      dp.emit();
    } catch (const std::exception& e) {
      std::cerr << "Workspaces: " << e.what() << std::endl;
    }
  };
}

auto waybar::modules::sway::Workspaces::update() -> void
{
  bool needReorder = false;
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = buttons_.begin(); it != buttons_.end();) {
    auto ws = std::find_if(workspaces_.begin(), workspaces_.end(),
      [it](auto node) -> bool { return node["name"].asString() == it->first; });
    if (ws == workspaces_.end() ||
            (!config_["all-outputs"].asBool() &&
             (*ws)["output"].asString() != bar_.output_name)) {
      it = buttons_.erase(it);
      needReorder = true;
    } else {
      ++it;
    }
  }
  for (auto const& node : workspaces_) {
    if (!config_["all-outputs"].asBool()
      && bar_.output_name != node["output"].asString()) {
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
        box_.reorder_child(button, node["num"].asInt());
      }
      auto icon = getIcon(node["name"].asString(), node);
      if (config_["format"].isString()) {
        auto format = config_["format"].asString();
        button.set_label(fmt::format(format, fmt::arg("icon", icon),
          fmt::arg("name", node["name"].asString()),
          fmt::arg("index", node["num"].asString())));
      } else {
        button.set_label(icon);
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
  auto icon = getIcon(node["name"].asString(), node);
  auto format = config_["format"].isString()
    ? fmt::format(config_["format"].asString(), fmt::arg("icon", icon),
      fmt::arg("name", node["name"].asString()),
      fmt::arg("index", node["num"].asString()))
    : icon;
  auto pair = buttons_.emplace(node["name"].asString(), format);
  auto &button = pair.first->second;
  box_.pack_start(button, false, false, 0);
  button.set_relief(Gtk::RELIEF_NONE);
  button.signal_clicked().connect([this, pair] {
    try {
      std::lock_guard<std::mutex> lock(mutex_);
      auto cmd = fmt::format("workspace \"{}\"", pair.first->first);
      ipc_.sendCmd(IPC_COMMAND, cmd);
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

std::string waybar::modules::sway::Workspaces::getIcon(std::string name,
  Json::Value node)
{
  std::vector<std::string> keys = {
    name, "urgent", "focused", "visible", "default"};
  for (auto const& key : keys) {
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

bool waybar::modules::sway::Workspaces::handleScroll(GdkEventScroll *e)
{
  // Avoid concurrent scroll event
  if (scrolling_) {
    return false;
  }
  scrolling_ = true;
  std::string name;
  uint16_t idx = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (; idx < workspaces_.size(); idx += 1) {
      if (workspaces_[idx]["focused"].asBool()) {
        name = workspaces_[idx]["name"].asString();
        break;
      }
    }
  }
  if (name.empty()) {
    scrolling_ = false;
    return false;
  }
  if (e->direction == GDK_SCROLL_UP) {
      name = getNextWorkspace();
  }
  if (e->direction == GDK_SCROLL_DOWN) {
      name = getPrevWorkspace();
  }
  if (e->direction == GDK_SCROLL_SMOOTH) {
    gdouble delta_x, delta_y;
    gdk_event_get_scroll_deltas(reinterpret_cast<const GdkEvent *>(e),
      &delta_x, &delta_y);
    if (delta_y < 0) {
      name = getNextWorkspace();
    } else if (delta_y > 0) {
      name = getPrevWorkspace();
    }
  }
  if (!name.empty()) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (name == workspaces_[idx]["name"].asString()) {
      scrolling_ = false;
      return false;
    }
    ipc_.sendCmd(IPC_COMMAND, fmt::format("workspace \"{}\"", name));
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
  }
  return true;
}

std::string waybar::modules::sway::Workspaces::getPrevWorkspace()
{
  for (uint16_t i = 0; i != workspaces_.size(); i += 1) {
    if (workspaces_[i]["focused"].asBool()) {
      if (i > 0) {
        return workspaces_[i - 1]["name"].asString();
      }
      return workspaces_[workspaces_.size() - 1]["name"].asString();
    }
  }
  return "";
}

std::string waybar::modules::sway::Workspaces::getNextWorkspace()
{
  for (uint16_t i = 0; i != workspaces_.size(); i += 1) {
    if (workspaces_[i]["focused"].asBool()) {
      if (i + 1U < workspaces_.size()) {
        return workspaces_[i + 1]["name"].asString();
      }
      return workspaces_[0]["String"].asString();
    }
  }
  return "";
}

waybar::modules::sway::Workspaces::operator Gtk::Widget &() {
  return box_;
}
