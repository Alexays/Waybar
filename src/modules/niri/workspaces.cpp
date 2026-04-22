#include "modules/niri/workspaces.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>

namespace waybar::modules::niri {

Workspaces::Workspaces(const std::string& id, const Bar& bar, const Json::Value& config)
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
  gIPC->registerForIPC("WorkspaceUrgencyChanged", this);

  gIPC->registerForIPC("WindowsChanged", this);
  gIPC->registerForIPC("WindowOpenedOrChanged", this);
  gIPC->registerForIPC("WindowLayoutsChanged", this);
  gIPC->registerForIPC("WindowFocusChanged", this);
  gIPC->registerForIPC("WindowClosed", this);
  gIPC->registerForIPC("WindowFocusChanged", this);

  dp.emit();
}

Workspaces::~Workspaces() {
  gIPC->unregisterForIPC(this);
}

void Workspaces::onEvent(const Json::Value& /*ev*/) { dp.emit(); }

void Workspaces::doUpdate() {
  auto ipcLock = gIPC->lockData();

  const bool alloutputs = config_["all-outputs"].asBool();
  const auto& all_workspaces = gIPC->workspaces();
  const auto& all_windows = gIPC->windows();

  std::vector<const Json::Value*> my_workspaces;
  my_workspaces.reserve(all_workspaces.size());
  for (const auto& ws : all_workspaces) {
    if (alloutputs || ws["output"].asString() == bar_.output->name) {
      my_workspaces.push_back(&ws);
    }
  }

  workspaces_.erase(
      std::remove_if(workspaces_.begin(), workspaces_.end(),
                     [&](const std::unique_ptr<Workspace>& w) {
                       bool gone =
                           std::none_of(my_workspaces.begin(), my_workspaces.end(),
                                        [&](const Json::Value* ws) {
                                          return ws->operator[]("id").asUInt64() == w->id();
                                        });
                       if (gone) box_.remove(w->button());
                       return gone;
                     }),
      workspaces_.end());

  for (const auto* ws_ptr : my_workspaces) {
    const auto& ws = *ws_ptr;
    const auto ws_id = ws.isMember("id") ? ws["id"].asUInt64() : 0;

    auto it = std::find_if(workspaces_.begin(), workspaces_.end(),
                           [ws_id](const std::unique_ptr<Workspace>& w) {
                             return w->id() == ws_id;
                           });

    if (it == workspaces_.end()) {
      createWorkspace(ws);
      it = workspaces_.end() - 1;
    }

    std::vector<Json::Value> windows_vec(all_windows.begin(), all_windows.end());
    (*it)->update(ws, windows_vec);
  }

  for (auto pos_it = my_workspaces.cbegin(); pos_it != my_workspaces.cend(); ++pos_it) {
    const auto& ws = **pos_it;
    const auto ws_id = ws.isMember("id") ? ws["id"].asUInt64() : 0;

    int pos = static_cast<int>(pos_it - my_workspaces.cbegin());
    if (alloutputs) {
    } else {
      pos = static_cast<int>(ws["idx"].asUInt()) - 1;
    }

    auto it = std::find_if(workspaces_.begin(), workspaces_.end(),
                           [ws_id](const std::unique_ptr<Workspace>& w) {
                             return w->id() == ws_id;
                           });
    if (it != workspaces_.end()) {
      box_.reorder_child((*it)->button(), pos);
    }
  }
}

void Workspaces::update() {
  doUpdate();
  AModule::update();
}


void Workspaces::createWorkspace(const Json::Value& workspace_data) {
  auto ws = std::make_unique<Workspace>(workspace_data, *this);
  box_.pack_start(ws->button(), false, false, 0);
  workspaces_.push_back(std::move(ws));
}

std::string Workspaces::getIcon(const std::string& value, const Json::Value& ws) const {
  const auto& icons = config_["format-icons"];
  if (!icons) return value;

  if (ws["is_urgent"].asBool() && icons["urgent"]) return icons["urgent"].asString();
  if (ws["is_active"].asBool() && icons["active"]) return icons["active"].asString();
  if (ws["is_focused"].asBool() && icons["focused"]) return icons["focused"].asString();
  if (ws["active_window_id"].isNull() && icons["empty"]) return icons["empty"].asString();

  if (ws["name"]) {
    const auto& name = ws["name"].asString();
    if (icons[name]) return icons[name].asString();
  }

  const auto idx = ws["idx"].asString();
  if (icons[idx]) return icons[idx].asString();

  if (icons["default"]) return icons["default"].asString();

  return value;
}

}  // namespace waybar::modules::niri