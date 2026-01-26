#include "modules/niri/workspaces.hpp"

#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>

namespace waybar::modules::niri {

Workspaces::Workspaces(const std::string &id, const Bar &bar, const Json::Value &config)
    : AModule(config, "workspaces", id, false, false), bar_(bar), box_(bar.orientation, 0) {
  const auto config_sort_by_number = config_["sort-by-number"];
  if (config_sort_by_number.isBool()) {
    spdlog::warn("[niri/workspaces]: Prefer sort-by-id instead of sort-by-number");
    sort_by_id_ = config_sort_by_number.asBool();
  }

  const auto config_sort_by_id = config_["sort-by-id"];
  if (config_sort_by_id.isBool()) {
    sort_by_id_ = config_sort_by_id.asBool();
  }

  const auto config_sort_by_name = config_["sort-by-name"];
  if (config_sort_by_name.isBool()) {
    sort_by_name_ = config_sort_by_name.asBool();
  }

  const auto config_sort_by_coordinates = config_["sort-by-coordinates"];
  if (config_sort_by_coordinates.isBool()) {
    sort_by_coordinates_ = config_sort_by_coordinates.asBool();
  }

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

  sortWorkspaces(my_workspaces);

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

    const auto pos = static_cast<int>(std::distance(my_workspaces.cbegin(), it));

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

void Workspaces::sortWorkspaces(std::vector<Json::Value> &workspaces) const {
  auto get_name = [](const Json::Value &ws) -> std::string {
    if (ws["name"]) return ws["name"].asString();
    return std::to_string(ws["idx"].asUInt());
  };

  auto is_numeric = [](const std::string &value) {
    return !value.empty() &&
           std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isdigit(c); });
  };

  const bool names_are_numeric =
      std::all_of(workspaces.begin(), workspaces.end(),
                  [&](const auto &ws) { return is_numeric(get_name(ws)); });

  auto compare_numeric_strings = [](const std::string &a, const std::string &b) {
    if (a.size() != b.size()) return a.size() < b.size();
    return a < b;
  };

  std::sort(workspaces.begin(), workspaces.end(), [&](const auto &a, const auto &b) {
    if (sort_by_id_) {
      return a["id"].asUInt64() < b["id"].asUInt64();
    }

    if (sort_by_name_) {
      const auto a_name = get_name(a);
      const auto b_name = get_name(b);
      if (a_name == b_name) return a["id"].asUInt64() < b["id"].asUInt64();
      if (names_are_numeric) return compare_numeric_strings(a_name, b_name);
      return a_name < b_name;
    }

    if (sort_by_coordinates_) {
      const auto &a_output = a["output"].asString();
      const auto &b_output = b["output"].asString();
      if (a_output == b_output) {
        const auto a_idx = a["idx"].asUInt();
        const auto b_idx = b["idx"].asUInt();
        if (a_idx == b_idx) return a["id"].asUInt64() < b["id"].asUInt64();
        return a_idx < b_idx;
      }
      return a_output < b_output;
    }

    // Default to sorting by workspace index on each output.
    const auto &a_output = a["output"].asString();
    const auto &b_output = b["output"].asString();
    const auto a_idx = a["idx"].asUInt();
    const auto b_idx = b["idx"].asUInt();
    if (a_output == b_output) return a_idx < b_idx;
    return a_output < b_output;
  });
}

}  // namespace waybar::modules::niri
