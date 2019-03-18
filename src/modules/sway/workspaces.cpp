#include "modules/sway/workspaces.hpp"

waybar::modules::sway::Workspaces::Workspaces(const std::string& id, const Bar& bar,
  const Json::Value& config)
  : bar_(bar), config_(config), scrolling_(false)
{
  box_.set_name("workspaces");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  ipc_.subscribe("[ \"workspace\" ]");
  // Launch worker
  worker();
}

void waybar::modules::sway::Workspaces::worker()
{
  thread_ = [this] {
    try {
      if (!workspaces_.empty()) {
        ipc_.handleEvent();
      }
      {
        std::lock_guard<std::mutex> lock(mutex_);
        auto res = ipc_.sendCmd(IPC_GET_WORKSPACES);
        if (thread_.isRunning()) {
          workspaces_ = parser_.parse(res.payload);
        }
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
            (!config_["all-outputs"].asBool() && (*ws)["output"].asString() != bar_.output_name)) {
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
        box_.reorder_child(button, getWorkspaceIndex(node["name"].asString()));
      }
      auto icon = getIcon(node["name"].asString(), node);
      if (config_["format"].isString()) {
        auto format = config_["format"].asString();
        button.set_label(fmt::format(format, fmt::arg("icon", icon),
          fmt::arg("name", trimWorkspaceName(node["name"].asString())),
          fmt::arg("index", node["num"].asString())));
      } else {
        button.set_label(icon);
      }
      onButtonReady(node, button);
    }
  }
  if (scrolling_) {
    scrolling_ = false;
  }
}

void waybar::modules::sway::Workspaces::addWorkspace(const Json::Value &node)
{
  auto icon = getIcon(node["name"].asString(), node);
  auto format = config_["format"].isString()
    ? fmt::format(config_["format"].asString(), fmt::arg("icon", icon),
      fmt::arg("name", trimWorkspaceName(node["name"].asString())),
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
  if (!config_["disable-scroll"].asBool()) {
    button.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
    button.signal_scroll_event()
      .connect(sigc::mem_fun(*this, &Workspaces::handleScroll));
  }
  box_.reorder_child(button, getWorkspaceIndex(node["name"].asString()));
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

std::string waybar::modules::sway::Workspaces::getIcon(const std::string &name,
  const Json::Value &node)
{
  std::vector<std::string> keys = { name, "urgent", "focused", "visible", "default" };
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
  std::lock_guard<std::mutex> lock(mutex_);
  uint8_t idx;
  scrolling_ = true;
  for (idx = 0; idx < workspaces_.size(); idx += 1) {
    if (workspaces_[idx]["focused"].asBool()) {
      break;
    }
  }
  if (idx == workspaces_.size()) {
    scrolling_ = false;
    return false;
  }
  std::string name;
  if (e->direction == GDK_SCROLL_UP) {
      name = getCycleWorkspace(idx, false);
  }
  if (e->direction == GDK_SCROLL_DOWN) {
      name = getCycleWorkspace(idx, true);
  }
  if (e->direction == GDK_SCROLL_SMOOTH) {
    gdouble delta_x, delta_y;
    gdk_event_get_scroll_deltas(reinterpret_cast<const GdkEvent *>(e),
      &delta_x, &delta_y);
    if (delta_y < 0) {
      name = getCycleWorkspace(idx, false);
    } else if (delta_y > 0) {
      name = getCycleWorkspace(idx, true);
    }
  }
  if (name.empty() || name == workspaces_[idx]["name"].asString()) {
    scrolling_ = false;
    return false;
  }
  ipc_.sendCmd(IPC_COMMAND, fmt::format("workspace \"{}\"", name));
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  return true;
}

const std::string waybar::modules::sway::Workspaces::getCycleWorkspace(
  uint8_t focused_workspace, bool prev) const
{
  auto inc = prev ? -1 : 1;
  int size = workspaces_.size();
  uint8_t idx = 0;
  for (int i = focused_workspace; i < size && i >= 0; i += inc) {
    bool same_output = (workspaces_[i]["output"].asString() == bar_.output_name
      && !config_["all-outputs"].asBool()) || config_["all-outputs"].asBool();
    bool same_name =
      workspaces_[i]["name"].asString() == workspaces_[focused_workspace]["name"].asString();
    if (same_output && !same_name) {
      return workspaces_[i]["name"].asString();
    }
    if (prev && i - 1 < 0) {
      i = size;
    } else if (!prev && i + 1 >= size) {
      i = -1;
    } else if (idx >= workspaces_.size()) {
      return "";
    }
    idx += 1;
  }
  return "";
}

uint16_t waybar::modules::sway::Workspaces::getWorkspaceIndex(const std::string &name) const
{
  uint16_t idx = 0;
  for (const auto &workspace : workspaces_) {
    if (workspace["name"].asString() == name) {
      return idx;
    }
    if (!(!config_["all-outputs"].asBool() && workspace["output"].asString() != bar_.output_name)) {
      idx += 1;
    }
  }
  return workspaces_.size();
}

std::string waybar::modules::sway::Workspaces::trimWorkspaceName(std::string name)
{
  std::size_t found = name.find(":");
  if (found!=std::string::npos) {
    return name.substr(found+1);
  }
  return name;
}

void waybar::modules::sway::Workspaces::onButtonReady(const Json::Value& node, Gtk::Button& button)
{
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

waybar::modules::sway::Workspaces::operator Gtk::Widget &() {
  return box_;
}
