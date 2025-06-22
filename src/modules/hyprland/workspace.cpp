#include <json/value.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <string>
#include <utility>

#include "modules/hyprland/workspaces.hpp"

namespace waybar::modules::hyprland {

Workspace::Workspace(const Json::Value &workspace_data, Workspaces &workspace_manager,
                     const Json::Value &clients_data)
    : m_workspaceManager(workspace_manager),
      m_id(workspace_data["id"].asInt()),
      m_name(workspace_data["name"].asString()),
      m_output(workspace_data["monitor"].asString()),  // TODO:allow using monitor desc
      m_windows(workspace_data["windows"].asInt()),
      m_isActive(true),
      m_isPersistentRule(workspace_data["persistent-rule"].asBool()),
      m_isPersistentConfig(workspace_data["persistent-config"].asBool()),
      m_ipc(IPC::inst()) {
  if (m_name.starts_with("name:")) {
    m_name = m_name.substr(5);
  } else if (m_name.starts_with("special")) {
    m_name = m_id == -99 ? m_name : m_name.substr(8);
    m_isSpecial = true;
  }

  m_button.add_events(Gdk::BUTTON_PRESS_MASK);
  m_button.signal_button_press_event().connect(sigc::mem_fun(*this, &Workspace::handleClicked),
                                               false);

  m_button.set_relief(Gtk::RELIEF_NONE);
  m_content.set_center_widget(m_label);
  m_button.add(m_content);

  initializeWindowMap(clients_data);
}

void addOrRemoveClass(const Glib::RefPtr<Gtk::StyleContext> &context, bool condition,
                      const std::string &class_name) {
  if (condition) {
    context->add_class(class_name);
  } else {
    context->remove_class(class_name);
  }
}

std::optional<std::string> Workspace::closeWindow(WindowAddress const &addr) {
  if (m_windowMap.contains(addr)) {
    return removeWindow(addr);
  }
  return std::nullopt;
}

bool Workspace::handleClicked(GdkEventButton *bt) const {
  if (bt->type == GDK_BUTTON_PRESS) {
    try {
      if (id() > 0) {  // normal
        if (m_workspaceManager.moveToMonitor()) {
          m_ipc.getSocket1Reply("dispatch focusworkspaceoncurrentmonitor " + std::to_string(id()));
        } else {
          m_ipc.getSocket1Reply("dispatch workspace " + std::to_string(id()));
        }
      } else if (!isSpecial()) {  // named (this includes persistent)
        if (m_workspaceManager.moveToMonitor()) {
          m_ipc.getSocket1Reply("dispatch focusworkspaceoncurrentmonitor name:" + name());
        } else {
          m_ipc.getSocket1Reply("dispatch workspace name:" + name());
        }
      } else if (id() != -99) {  // named special
        m_ipc.getSocket1Reply("dispatch togglespecialworkspace " + name());
      } else {  // special
        m_ipc.getSocket1Reply("dispatch togglespecialworkspace");
      }
      return true;
    } catch (const std::exception &e) {
      spdlog::error("Failed to dispatch workspace: {}", e.what());
    }
  }
  return false;
}

void Workspace::initializeWindowMap(const Json::Value &clients_data) {
  m_windowMap.clear();
  for (auto client : clients_data) {
    if (client["workspace"]["id"].asInt() == id()) {
      insertWindow({client});
    }
  }
}

void Workspace::insertWindow(WindowCreationPayload create_window_payload) {
  if (!create_window_payload.isEmpty(m_workspaceManager)) {
    auto repr = create_window_payload.repr(m_workspaceManager);

    if (!repr.empty()) {
      m_windowMap[create_window_payload.getAddress()] = repr;
    }
  }
};

bool Workspace::onWindowOpened(WindowCreationPayload const &create_window_payload) {
  if (create_window_payload.getWorkspaceName() == name()) {
    insertWindow(create_window_payload);
    return true;
  }
  return false;
}

std::string Workspace::removeWindow(WindowAddress const &addr) {
  std::string windowRepr = m_windowMap[addr];
  m_windowMap.erase(addr);
  return windowRepr;
}

std::string &Workspace::selectIcon(std::map<std::string, std::string> &icons_map) {
  spdlog::trace("Selecting icon for workspace {}", name());
  if (isUrgent()) {
    auto urgentIconIt = icons_map.find("urgent");
    if (urgentIconIt != icons_map.end()) {
      return urgentIconIt->second;
    }
  }

  if (isActive()) {
    auto activeIconIt = icons_map.find("active");
    if (activeIconIt != icons_map.end()) {
      return activeIconIt->second;
    }
  }

  if (isSpecial()) {
    auto specialIconIt = icons_map.find("special");
    if (specialIconIt != icons_map.end()) {
      return specialIconIt->second;
    }
  }

  auto namedIconIt = icons_map.find(name());
  if (namedIconIt != icons_map.end()) {
    return namedIconIt->second;
  }

  if (isVisible()) {
    auto visibleIconIt = icons_map.find("visible");
    if (visibleIconIt != icons_map.end()) {
      return visibleIconIt->second;
    }
  }

  if (isEmpty()) {
    auto emptyIconIt = icons_map.find("empty");
    if (emptyIconIt != icons_map.end()) {
      return emptyIconIt->second;
    }
  }

  if (isPersistent()) {
    auto persistentIconIt = icons_map.find("persistent");
    if (persistentIconIt != icons_map.end()) {
      return persistentIconIt->second;
    }
  }

  auto defaultIconIt = icons_map.find("default");
  if (defaultIconIt != icons_map.end()) {
    return defaultIconIt->second;
  }

  return m_name;
}

void Workspace::update(const std::string &format, const std::string &icon) {
  if (this->m_workspaceManager.persistentOnly() && !this->isPersistent()) {
    m_button.hide();
    return;
  }
  // clang-format off
  if (this->m_workspaceManager.activeOnly() && \
     !this->isActive() && \
     !this->isPersistent() && \
     !this->isVisible() && \
     !this->isSpecial()) {
    // clang-format on
    // if activeOnly is true, hide if not active, persistent, visible or special
    m_button.hide();
    return;
  }
  if (this->m_workspaceManager.specialVisibleOnly() && this->isSpecial() && !this->isVisible()) {
    m_button.hide();
    return;
  }
  m_button.show();

  auto styleContext = m_button.get_style_context();
  addOrRemoveClass(styleContext, isActive(), "active");
  addOrRemoveClass(styleContext, isSpecial(), "special");
  addOrRemoveClass(styleContext, isEmpty(), "empty");
  addOrRemoveClass(styleContext, isPersistent(), "persistent");
  addOrRemoveClass(styleContext, isUrgent(), "urgent");
  addOrRemoveClass(styleContext, isVisible(), "visible");
  addOrRemoveClass(styleContext, m_workspaceManager.getBarOutput() == output(), "hosting-monitor");

  std::string windows;
  auto windowSeparator = m_workspaceManager.getWindowSeparator();

  bool isNotFirst = false;

  for (auto &[_pid, window_repr] : m_windowMap) {
    if (isNotFirst) {
      windows.append(windowSeparator);
    }
    isNotFirst = true;
    windows.append(window_repr);
  }

  m_label.set_markup(fmt::format(fmt::runtime(format), fmt::arg("id", id()),
                                 fmt::arg("name", name()), fmt::arg("icon", icon),
                                 fmt::arg("windows", windows)));
}

}  // namespace waybar::modules::hyprland
