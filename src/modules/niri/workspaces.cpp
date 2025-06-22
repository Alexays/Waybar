#include "modules/niri/workspaces.hpp"

#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <spdlog/spdlog.h>

namespace waybar::modules::niri {

Workspaces::Workspaces(const std::string &id, const Bar &bar, const Json::Value &config)
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

  dp.emit();
}

Workspaces::~Workspaces() { gIPC->unregisterForIPC(this); }

void Workspaces::onEvent(const Json::Value &ev) { dp.emit(); }

void Workspaces::doUpdate() {
  auto ipcLock = gIPC->lockData();

  const auto alloutputs = config_["all-outputs"].asBool();
  std::vector<Json::Value> my_workspaces;
  const auto &workspaces = gIPC->workspaces();
  std::copy_if(workspaces.cbegin(), workspaces.cend(), std::back_inserter(my_workspaces),
               [&](const auto &ws) {
                 if (alloutputs) return true;
                 return ws["output"].asString() == bar_.output->name;
               });

  // Remove buttons for removed workspaces.
  for (auto it = buttons_.begin(); it != buttons_.end();) {
    auto ws = std::find_if(my_workspaces.begin(), my_workspaces.end(),
                           [it](const auto &ws) { return ws["id"].asUInt64() == it->first; });
    if (ws == my_workspaces.end()) {
      it = buttons_.erase(it);
    } else {
      ++it;
    }
  }

  // Add buttons for new workspaces, update existing ones.
  for (const auto &ws : my_workspaces) {
    auto bit = buttons_.find(ws["id"].asUInt64());
    auto &button = bit == buttons_.end() ? addButton(ws) : bit->second;
    auto style_context = button.get_style_context();

    if (ws["is_focused"].asBool())
      style_context->add_class("focused");
    else
      style_context->remove_class("focused");

    if (ws["is_active"].asBool())
      style_context->add_class("active");
    else
      style_context->remove_class("active");

    if (ws["is_urgent"].asBool())
      style_context->add_class("urgent");
    else
      style_context->remove_class("urgent");

    if (ws["output"]) {
      if (ws["output"].asString() == bar_.output->name)
        style_context->add_class("current_output");
      else
        style_context->remove_class("current_output");
    } else {
      style_context->remove_class("current_output");
    }

    if (ws["active_window_id"].isNull())
      style_context->add_class("empty");
    else
      style_context->remove_class("empty");

    std::string name;
    if (ws["name"]) {
      name = ws["name"].asString();
    } else {
      name = std::to_string(ws["idx"].asUInt());
    }
    button.set_name("niri-workspace-" + name);

    if (config_["format"].isString()) {
      auto format = config_["format"].asString();
      name = fmt::format(fmt::runtime(format), fmt::arg("icon", getIcon(name, ws)),
                         fmt::arg("value", name), fmt::arg("name", ws["name"].asString()),
                         fmt::arg("index", ws["idx"].asUInt()),
                         fmt::arg("output", ws["output"].asString()));
    }
    if (!config_["disable-markup"].asBool()) {
      static_cast<Gtk::Label *>(button.get_children()[0])->set_markup(name);
    } else {
      button.set_label(name);
    }

    if (config_["current-only"].asBool()) {
      const auto *property = alloutputs ? "is_focused" : "is_active";
      if (ws[property].asBool())
        button.show();
      else
        button.hide();
    } else {
      button.show();
    }
  }

  // Refresh the button order.
  for (auto it = my_workspaces.cbegin(); it != my_workspaces.cend(); ++it) {
    const auto &ws = *it;

    auto pos = ws["idx"].asUInt() - 1;
    if (alloutputs) pos = it - my_workspaces.cbegin();

    auto &button = buttons_[ws["id"].asUInt64()];
    box_.reorder_child(button, pos);
  }
}

void Workspaces::update() {
  doUpdate();
  AModule::update();
}

Gtk::Button &Workspaces::addButton(const Json::Value &ws) {
  std::string name;
  if (ws["name"]) {
    name = ws["name"].asString();
  } else {
    name = std::to_string(ws["idx"].asUInt());
  }

  auto pair = buttons_.emplace(ws["id"].asUInt64(), name);
  auto &&button = pair.first->second;
  box_.pack_start(button, false, false, 0);
  button.set_relief(Gtk::RELIEF_NONE);
  if (!config_["disable-click"].asBool()) {
    const auto id = ws["id"].asUInt64();
    button.signal_pressed().connect([=] {
      try {
        // {"Action":{"FocusWorkspace":{"reference":{"Id":1}}}}
        Json::Value request(Json::objectValue);
        auto &action = (request["Action"] = Json::Value(Json::objectValue));
        auto &focusWorkspace = (action["FocusWorkspace"] = Json::Value(Json::objectValue));
        auto &reference = (focusWorkspace["reference"] = Json::Value(Json::objectValue));
        reference["Id"] = id;

        IPC::send(request);
      } catch (const std::exception &e) {
        spdlog::error("Error switching workspace: {}", e.what());
      }
    });
  }
  return button;
}

std::string Workspaces::getIcon(const std::string &value, const Json::Value &ws) {
  const auto &icons = config_["format-icons"];
  if (!icons) return value;

  if (ws["is_urgent"].asBool() && icons["urgent"]) return icons["urgent"].asString();

  if (ws["active_window_id"].isNull() && icons["empty"]) return icons["empty"].asString();

  if (ws["is_focused"].asBool() && icons["focused"]) return icons["focused"].asString();

  if (ws["is_active"].asBool() && icons["active"]) return icons["active"].asString();

  if (ws["name"]) {
    const auto &name = ws["name"].asString();
    if (icons[name]) return icons[name].asString();
  }

  const auto idx = ws["idx"].asString();
  if (icons[idx]) return icons[idx].asString();

  if (icons["default"]) return icons["default"].asString();

  return value;
}

}  // namespace waybar::modules::niri
