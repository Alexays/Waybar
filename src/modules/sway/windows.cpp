#include "modules/sway/windows.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <string>

//TODO Integrate with window class.

namespace waybar::modules::sway {

Windows::Windows(const std::string &id, const Bar &bar, const Json::Value &config)
    : AModule(config, "windows", id, false, !config["disable-scroll"].asBool()),
      bar_(bar),
      box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0) {
  box_.set_name("windows");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  event_box_.add(box_);
  ipc_.subscribe(R"(["window", "workspace"])");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &Windows::onEvent));
  ipc_.signal_cmd.connect(sigc::mem_fun(*this, &Windows::onCmd));
  ipc_.sendCmd(IPC_GET_WORKSPACES);
  ipc_.sendCmd(IPC_GET_TREE);
  if (config["enable-bar-scroll"].asBool()) {
    auto &window = const_cast<Bar &>(bar_).window;
    window.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
    window.signal_scroll_event().connect(sigc::mem_fun(*this, &Windows::handleScroll));
  }
  // Launch worker
  ipc_.setWorker([this] {
    try {
      ipc_.handleEvent();
    } catch (const std::exception &e) {
      spdlog::error("Windows: {}", e.what());
    }
  });
}

void Windows::onEvent(const struct Ipc::ipc_response &res) {
  try {
    //For some reason, the "tree" shows all workspaces as "unfocused", at least for me.
    //So we need to determine the currently focused workspace first using an additional command.
    ipc_.sendCmd(IPC_GET_WORKSPACES);
    ipc_.sendCmd(IPC_GET_TREE);
  } catch (const std::exception &e) {
    spdlog::error("Windows: {}", e.what());
  }
}

void Windows::onCmd(const struct Ipc::ipc_response &res) {
  if (res.type == IPC_GET_WORKSPACES)  {
    try {
      {
        std::lock_guard<std::mutex> lock(mutex_);
    	auto payload = parser_.parse(res.payload);
	workspace_ = (* std::find_if(payload.begin(), payload.end(),
		[&](const auto &workspace) {
			return workspace["focused"].asBool();
		}));
      }
      dp.emit();
    } catch (const std::exception &e) {
      spdlog::error("Windows: {}", e.what());
    }
  } else if (res.type == IPC_GET_TREE) {
    try {
      {
        std::lock_guard<std::mutex> lock(mutex_);
	//There is only one root node.
        auto payload = parser_.parse(res.payload)["nodes"];
	//The subtree that represents the output the bar is being drawn on. An "all-outputs" setting does not make sense here.
        auto our_output_tree = (* std::find_if(payload.begin(), payload.end(),
                     [&](const auto &output) {
                       return output["name"].asString() == bar_.output->name;
                     }))["nodes"];
	//Essentially the same, only now we are looking for the node tree of the currently focused workspace.
	
	//TODO Multi-layout support (such as adding floating nodes to the pile if the config says so)
	//Right now, floating nodes will be entirely ignored, we simply pick the first layout
	//on this workspace and go from there. This is dirty and needs changing.
        auto our_workspace_tree = (* std::find_if(our_output_tree.begin(), our_output_tree.end(),
                     [&](const auto &workspace) {
			     return workspace["id"].asInt() == workspace_["id"].asInt();
		     }))["nodes"][0]["nodes"];
        windows_.clear();
	std::copy(our_workspace_tree.begin(), our_workspace_tree.end(), std::back_inserter(windows_));
      }
      dp.emit();
    } catch (const std::exception &e) {
      spdlog::error("Windows: {}", e.what());
    }
  }
}

bool Windows::filterButtons() {
  bool needReorder = false;
  for (auto it = buttons_.begin(); it != buttons_.end();) {
    auto win = std::find_if(windows_.begin(), windows_.end(),
                           [it](const auto &node) { return node["id"].asInt() == it->first; });
    if (win == windows_.end() || (*win)["output"].asString() != bar_.output->name) {
      it = buttons_.erase(it);
      needReorder = true;
    } else {
      ++it;
    }
  }
  return needReorder;
}

auto Windows::update() -> void {
  std::lock_guard<std::mutex> lock(mutex_);
  bool needReorder = filterButtons();
  for (auto it = windows_.begin(); it != windows_.end(); ++it) {
    auto bit = buttons_.find((*it)["id"].asInt());
    if (bit == buttons_.end()) {
      needReorder = true;
    }
    auto &button = bit == buttons_.end() ? addButton(*it) : bit->second;
    if ((*it)["focused"].asBool()) {
      button.get_style_context()->add_class("focused");
    } else {
      button.get_style_context()->remove_class("focused");
    }
    if ((*it)["visible"].asBool()) {
      button.get_style_context()->add_class("visible");
    } else {
      button.get_style_context()->remove_class("visible");
    }
    if ((*it)["sticky"].asBool()) {
      button.get_style_context()->add_class("sticky");
    } else {
      button.get_style_context()->remove_class("sticky");
    }
    if ((*it)["urgent"].asBool()) {
      button.get_style_context()->add_class("urgent");
    } else {
      button.get_style_context()->remove_class("urgent");
    }
    if (needReorder) {
      box_.reorder_child(button, it - windows_.begin());
    }
    std::string output = (*it)["name"].asString();
    if (config_["format"].isString()) {
      auto format = config_["format"].asString();
      output = fmt::format(format, fmt::arg("icon", getIcon(output, *it)),
                           fmt::arg("value", output), fmt::arg("name", trimWindowName(output)),
                           fmt::arg("index", (*it)["num"].asString()));
    } else {
      output = trimWindowName(output);
    }
    if (!config_["disable-markup"].asBool()) {
      static_cast<Gtk::Label *>(button.get_children()[0])->set_markup(output);
    } else {
      button.set_label(output);
    }
    onButtonReady(*it, button);
  }
  // Call parent update
  AModule::update();
}

Gtk::Button &Windows::addButton(const Json::Value &node) {
  auto pair = buttons_.emplace(node["id"].asInt(), node["title"].asString());
  auto &&button = pair.first->second;
  box_.pack_start(button, false, false, 0);
  button.set_name("sway-window-" + node["id"].asString());
  button.set_relief(Gtk::RELIEF_NONE);
  if (!config_["disable-click"].asBool()) {
    button.signal_pressed().connect([this, node] {
      try {
        ipc_.sendCmd(IPC_COMMAND, fmt::format(window_switch_cmd_,
                                                node["id"].asInt()));
      } catch (const std::exception &e) {
        spdlog::error("Windows: {}", e.what());
      }
    });
  }
  return button;
}

//TODO make this better
std::string Windows::getIcon(const std::string &name, const Json::Value &node) {
  if (node["app_id"].isNull()){
    if (node.isMember("window_properties")) {
      auto cls = node["window_properties"]["class"];
      if (!cls.isNull() && config_["format-icons"][cls.asString()].isString()) {
        return config_["format-icons"][cls.asString()].asString();
      }
    }
  } else if (config_["format-icons"].isMember(node["app_id"].asString())){
    return config_["format-icons"][node["app_id"].asString()].asString();
  }
  return config_["format-icons"]["default"].asString();
}

bool Windows::handleScroll(GdkEventScroll *e) {
  if (gdk_event_get_pointer_emulated((GdkEvent *)e)) {
    /**
     * Ignore emulated scroll events on window
     */
    return false;
  }
  auto dir = AModule::getScrollDir(e);
  if (dir == SCROLL_DIR::NONE) {
    return true;
  }
  int id;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(windows_.begin(), windows_.end(),
                           [](const auto &window) { return window["focused"].asBool(); });
    if (it == windows_.end()) {
      return true;
    }
    if (dir == SCROLL_DIR::DOWN || dir == SCROLL_DIR::RIGHT) {
      id = getCycleWindow(it, false);
    } else if (dir == SCROLL_DIR::UP || dir == SCROLL_DIR::LEFT) {
      id = getCycleWindow(it, true);
    } else {
      return true;
    }
    if (id == (*it)["id"].asInt()) {
      return true;
    }
  }
  try {
    ipc_.sendCmd(IPC_COMMAND, fmt::format(window_switch_cmd_, id));
  } catch (const std::exception &e) {
    spdlog::error("Windows: {}", e.what());
  }
  return true;
}

const int Windows::getCycleWindow(std::vector<Json::Value>::iterator it,
                                                bool prev) const {
  if (prev && it == windows_.begin() && !config_["disable-scroll-wraparound"].asBool()) {
    return (*(--windows_.end()))["id"].asInt();
  }
  if (prev && it != windows_.begin())
    --it;
  else if (!prev && it != windows_.end())
    ++it;
  if (!prev && it == windows_.end()) {
    if (config_["disable-scroll-wraparound"].asBool()) {
      --it;
    } else {
      return (*(windows_.begin()))["id"].asInt();
    }
  }
  return (*it)["id"].asInt();
}

std::string Windows::trimWindowName(std::string name) {
  if (config_["max-title-length"].asInt() >= 0){
    return name.substr(config_["max-title-length"].asInt());
  }
  return name;
}

void Windows::onButtonReady(const Json::Value &node, Gtk::Button &button) {
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

}  // namespace waybar::modules::sway
