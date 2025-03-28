#include "modules/niri/taskbar.hpp"

#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <spdlog/spdlog.h>

namespace waybar::modules::niri {

Taskbar::Taskbar(const std::string &id, const Bar &bar, const Json::Value &config)
    : AModule(config, "workspaces", id, false, false), bar_(bar), box_(bar.orientation, 0) {
  box_.set_name("workspaces");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(MODULE_CLASS);
  event_box_.add(box_);

  if (!gIPC) gIPC = std::make_unique<IPC>();

  gIPC->registerForIPC("WorkspacesChanged", this);
  gIPC->registerForIPC("WorkspaceActivated", this);
  gIPC->registerForIPC("WorkspaceActiveWindowChanged", this);

  dp.emit();
}

Taskbar::~Taskbar() { gIPC->unregisterForIPC(this); }

void Taskbar::onEvent(const Json::Value &ev) { dp.emit(); }

void Taskbar::doUpdate() {
  auto ipcLock = gIPC->lockData();

  // Get my workspace id
  std::vector<Json::Value> my_workspaces;
  const auto &workspaces = gIPC->workspaces();
  auto my_workspace_id = 0;
  auto my_workspace_iter = std::ranges::find_if(
      workspaces,
      [&](const auto &ws) { // Get ws idx for active ws on same display as bar.
        bool ws_on_my_output = ws["output"].asString() == bar_.output->name;
        bool ws_is_active = ws["is_active"].asBool();
        return (ws_on_my_output && ws_is_active);
      });
  if (my_workspace_iter != std::ranges::end(workspaces)) {
    my_workspace_id = (*my_workspace_iter)["id"].asUInt();
  } else {
    spdlog::error("Failed to find workspace for current display output?");
  }
  spdlog::info("Updating buttons on display {}, workspace {}", bar_.output->name, my_workspace_id);

  // Get windows in my workspace idx
  std::vector<Json::Value> my_windows;
  const auto &windows = gIPC->windows();
  std::ranges::copy_if(
      windows,
      std::back_inserter(my_windows),
      [&](const auto &win) {
        if (win["workspace_id"].asInt() == my_workspace_id) {
        }
        return win["workspace_id"].asInt() == my_workspace_id;
      });

  // Remove buttons for windows no longer on display (closed, moved, or ws changed).
  for (auto button_iter = buttons_.begin(); button_iter != buttons_.end();) {
    auto win_iter = std::ranges::find_if(
        my_windows,
        [button_iter](const auto &win) {
          return win["pid"].asUInt64() == button_iter->first;
        });

    if (win_iter == my_windows.end()) {
      // window closed
      button_iter = buttons_.erase(button_iter);
    }
    else if ((*win_iter)["workspace_id"] != my_workspace_id) {
      // window exists, but left workspace
      button_iter = buttons_.erase(button_iter);
    }
    else {
      button_iter++;
    }
  }

  // Setup buttons and styles
  for (const auto &win : my_windows) {
    auto bit = buttons_.find(win["pid"].asUInt());
    auto &button = bit == buttons_.end() ? addButton(win) : bit->second;
    auto style_context = button.get_style_context();

    if (win["is_focused"].asBool())
      style_context->add_class("focused");
    else
      style_context->remove_class("focused");

    std::string name = std::to_string(win["pid"].asUInt());
    button.set_name("niri-workspace-" + name);

    //if (config_["format"].isString()) {
    //  auto format = config_["format"].asString();
    //  name = fmt::format(fmt::runtime(format), fmt::arg("icon", getIcon(name, win)),
    //                     fmt::arg("value", name), fmt::arg("name", win["name"].asString()),
    //                     fmt::arg("index", win["idx"].asUInt()),
    //                     fmt::arg("output", win["output"].asString()));
    //}
    button.set_label(win["app_id"].asString());
    spdlog::info("Created button for " + win["app_id"].asString());
    button.show();
  }

  // Refresh the button order.
  int pos = 0;
  for (auto& win : my_windows) {
    auto &button = buttons_[win["pid"].asUInt64()];
    box_.reorder_child(button, pos++);
  }
}

void Taskbar::update() {
  doUpdate();
  AModule::update();
}

Gtk::Button &Taskbar::addButton(const Json::Value &win) {
  std::string name = std::to_string(win["pid"].asUInt());

  auto pair = buttons_.emplace(win["pid"].asUInt64(), name);
  auto &&button = pair.first->second;
  box_.pack_start(button, false, false, 0);
  button.set_relief(Gtk::RELIEF_NONE);
  if (!config_["disable-click"].asBool()) {
    // TODO(luna) make for window focus?
    //const auto id = win["id"].asUInt64();
    //button.signal_pressed().connect([=] {
    //  try {
    //    // {"Action":{"FocusWorkspace":{"reference":{"Id":1}}}}
    //    Json::Value request(Json::objectValue);
    //    auto &action = (request["Action"] = Json::Value(Json::objectValue));
    //    auto &focusWorkspace = (action["FocusWorkspace"] = Json::Value(Json::objectValue));
    //    auto &reference = (focusWorkspace["reference"] = Json::Value(Json::objectValue));
    //    reference["Id"] = id;

    //    IPC::send(request);
    //  } catch (const std::exception &e) {
    //    spdlog::error("Error switching workspace: {}", e.what());
    //  }
    //});
  }
  return button;
}

std::string Taskbar::getIcon(const std::string &value, const Json::Value &win) {
  // TODO(luna) this code is dead
  //            to update with xdg application icons based on the wlr/taskbar implementation
  const auto &icons = config_["format-icons"];
  if (!icons) return value;

  if (win["is_focused"].asBool() && icons["focused"]) return icons["focused"].asString();

  if (win["is_active"].asBool() && icons["active"]) return icons["active"].asString();

  if (win["name"]) {
    const auto &name = win["name"].asString();
    if (icons[name]) return icons[name].asString();
  }

  const auto idx = win["idx"].asString();
  if (icons[idx]) return icons[idx].asString();

  if (icons["default"]) return icons["default"].asString();

  return value;
}

}  // namespace waybar::modules::niri
