#include <glibmm/main.h>
#include <json/value.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <utility>

#include "modules/hyprland/workspaces.hpp"
#include "util/command.hpp"
#include "util/icon_loader.hpp"

namespace {
constexpr std::string_view kCssClassPrefix = "ws-";

// Convert a workspace name to a valid CSS class name.
// Lowercases, replaces non-alphanumeric runs with single hyphens,
// and prefixes digit-leading names (CSS classes can't start with a digit).
std::string sanitizeCssClass(const std::string& name) {
  std::string result;
  result.reserve(name.size() + kCssClassPrefix.size());
  for (auto c : name) {
    auto uc = static_cast<unsigned char>(c);
    if (std::isalnum(uc)) {
      result += static_cast<char>(std::tolower(uc));
    } else if (!result.empty() && result.back() != '-') {
      result += '-';
    }
  }
  if (!result.empty() && result.back() == '-') {
    result.pop_back();
  }
  if (!result.empty() && std::isdigit(static_cast<unsigned char>(result.front()))) {
    result.insert(0, kCssClassPrefix);
  }
  return result;
}
}  // namespace

namespace waybar::modules::hyprland {

Workspace::Workspace(const Json::Value& workspace_data, Workspaces& workspace_manager,
                     const Json::Value& clients_data)
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
  m_button.add_events(Gdk::ENTER_NOTIFY_MASK | Gdk::LEAVE_NOTIFY_MASK);

  m_button.signal_enter_notify_event().connect(sigc::mem_fun(*this, &Workspace::handleEnter));
  m_button.signal_leave_notify_event().connect(sigc::mem_fun(*this, &Workspace::handleLeave));

  m_button.signal_button_press_event().connect(sigc::mem_fun(*this, &Workspace::handleClicked),
                                               false);

  m_button.set_relief(Gtk::RELIEF_NONE);
  if (m_workspaceManager.enableTaskbar()) {
    m_content.set_orientation(m_workspaceManager.taskbarOrientation());
    m_content.pack_start(m_labelBefore, false, false);
  } else {
    m_content.set_center_widget(m_labelBefore);
  }
  m_button.add(m_content);

  initializeWindowMap(clients_data);
}

Workspace::~Workspace() {
  // Disconnect the hover-check timeout so it can't fire on this destroyed
  // instance (Workspaces are removed at runtime while a check may be armed).
  stopHoverCheck();
}

void addOrRemoveClass(const Glib::RefPtr<Gtk::StyleContext>& context, bool condition,
                      const std::string& class_name) {
  if (condition) {
    context->add_class(class_name);
  } else {
    context->remove_class(class_name);
  }
}

std::optional<WindowRepr> Workspace::closeWindow(WindowAddress const& addr) {
  auto it = std::ranges::find_if(m_windowMap,
                                 [&addr](const auto& window) { return window.address == addr; });
  // If the vector contains the address, remove it and return the window representation
  if (it != m_windowMap.end()) {
    WindowRepr windowRepr = *it;
    m_windowMap.erase(it);
    return windowRepr;
  }
  return std::nullopt;
}

bool Workspace::pointerInsideButton() {
  auto display = Gdk::Display::get_default();
  if (!display) {
    return false;
  }

  auto seat = display->get_default_seat();
  if (!seat) {
    return false;
  }

  auto pointer = seat->get_pointer();
  if (!pointer) {
    return false;
  }

  Glib::RefPtr<Gdk::Screen> screen;
  int pointerRootX = 0;
  int pointerRootY = 0;

  pointer->get_position(screen, pointerRootX, pointerRootY);

  Gtk::Widget* toplevel = m_button.get_toplevel();
  if (toplevel == nullptr || !toplevel->get_window()) {
    return false;
  }

  int buttonX = 0;
  int buttonY = 0;

  if (!m_button.translate_coordinates(*toplevel, 0, 0, buttonX, buttonY)) {
    return false;
  }

  int windowRootX = 0;
  int windowRootY = 0;
  toplevel->get_window()->get_root_origin(windowRootX, windowRootY);

  const auto allocation = m_button.get_allocation();

  const int buttonRootX = windowRootX + buttonX;
  const int buttonRootY = windowRootY + buttonY;
  const int buttonWidth = allocation.get_width();
  const int buttonHeight = allocation.get_height();

  return pointerRootX >= buttonRootX && pointerRootY >= buttonRootY &&
         pointerRootX < buttonRootX + buttonWidth && pointerRootY < buttonRootY + buttonHeight;
}

bool Workspace::syncHoverClass() {
  auto styleContext = m_button.get_style_context();

  if (pointerInsideButton()) {
    styleContext->add_class("workspace-hover");
    return true;
  }

  styleContext->remove_class("workspace-hover");
  stopHoverCheck();
  return false;
}

void Workspace::startHoverCheck() {
  if (m_hoverCheckConnection.connected()) {
    return;
  }

  m_hoverCheckConnection =
      Glib::signal_timeout().connect(sigc::mem_fun(*this, &Workspace::syncHoverClass), 50);
}

void Workspace::stopHoverCheck() {
  if (m_hoverCheckConnection.connected()) {
    m_hoverCheckConnection.disconnect();
  }
}

bool Workspace::handleEnter(GdkEventCrossing* /*event*/) {
  m_button.get_style_context()->add_class("workspace-hover");
  startHoverCheck();
  return false;
}

bool Workspace::handleLeave(GdkEventCrossing* /*event*/) {
  /*
   * Do not remove immediately.
   * Workspace taskbar children can fire misleading leave events while the
   * pointer is still visually inside the workspace button.
   *
   * The polling check will remove the class once the pointer really leaves.
   */
  startHoverCheck();
  return false;
}
bool Workspace::handleClicked(GdkEventButton* bt) const {
  if (bt->type == GDK_BUTTON_PRESS) {
    try {
      if (id() > 0) {  // normal
        if (m_workspaceManager.moveToMonitor()) {
          IPC::dispatch("focusworkspaceoncurrentmonitor", std::to_string(id()));
        } else {
          IPC::dispatch("workspace", std::to_string(id()));
        }
      } else if (!isSpecial()) {  // named (this includes persistent)
        if (m_workspaceManager.moveToMonitor()) {
          IPC::dispatch("focusworkspaceoncurrentmonitor", "name:" + name());
        } else {
          IPC::dispatch("workspace", "name:" + name());
        }
      } else if (id() != -99) {  // named special
        IPC::dispatch("togglespecialworkspace", name());
      } else {  // special
        IPC::dispatch("togglespecialworkspace", "");
      }
      return true;
    } catch (const std::exception& e) {
      spdlog::error("Failed to dispatch workspace: {}", e.what());
    }
  }
  return false;
}

void Workspace::initializeWindowMap(const Json::Value& clients_data) {
  m_windowMap.clear();
  for (const auto& client : clients_data) {
    if (client["workspace"]["id"].asInt() == id()) {
      insertWindow({client});
    }
  }
}

void Workspace::setActiveWindow(WindowAddress const& addr) {
  std::optional<long> activeIdx;
  for (size_t i = 0; i < m_windowMap.size(); ++i) {
    auto& window = m_windowMap[i];
    bool isActive = (window.address == addr);
    window.setActive(isActive);
    if (isActive) {
      activeIdx = i;
    }
  }

  auto activeWindowPos = m_workspaceManager.activeWindowPosition();
  const bool has_active_window =
      activeIdx.has_value() && activeWindowPos != Workspaces::ActiveWindowPosition::NONE;

  if (has_active_window) {
    auto window = std::move(m_windowMap[*activeIdx]);
    m_windowMap.erase(m_windowMap.begin() + *activeIdx);
    if (activeWindowPos == Workspaces::ActiveWindowPosition::FIRST) {
      m_windowMap.insert(m_windowMap.begin(), std::move(window));
    } else if (activeWindowPos == Workspaces::ActiveWindowPosition::LAST) {
      m_windowMap.emplace_back(std::move(window));
    }
  }
}

void Workspace::insertWindow(WindowCreationPayload create_window_payload) {
  if (!create_window_payload.isEmpty(m_workspaceManager)) {
    auto repr = create_window_payload.repr(m_workspaceManager);

    const bool should_display = !repr.empty() || m_workspaceManager.enableTaskbar();

    if (should_display) {
      auto addr = create_window_payload.getAddress();
      auto it = std::ranges::find_if(
          m_windowMap, [&addr](const auto& window) { return window.address == addr; });
      // If the vector contains the address, update the window representation, otherwise insert it
      if (it != m_windowMap.end()) {
        *it = repr;
      } else {
        m_windowMap.emplace_back(repr);
      }
    }
  }
}

bool Workspace::onWindowOpened(WindowCreationPayload const& create_window_payload) {
  if (create_window_payload.getWorkspaceName() == name()) {
    insertWindow(create_window_payload);
    return true;
  }
  return false;
}

std::string& Workspace::selectIcon(std::map<std::string, std::string>& icons_map) {
  spdlog::trace("Selecting icon for workspace {}", name());
  if (isUrgent()) {
    auto urgentNamedIconIt = icons_map.find("urgent:" + name());
    if (urgentNamedIconIt != icons_map.end()) {
      return urgentNamedIconIt->second;
    }

    auto urgentIconIt = icons_map.find("urgent");
    if (urgentIconIt != icons_map.end()) {
      return urgentIconIt->second;
    }
  }

  if (isActive() && isSpecial()) {
    auto activeIconIt = icons_map.find("active:" + name());
    if (activeIconIt != icons_map.end()) {
      return activeIconIt->second;
    }
    auto namedIconIt = icons_map.find(name());
    if (namedIconIt != icons_map.end()) {
      return namedIconIt->second;
    }
  }

  if (isActive()) {
    auto activeNamedIconIt = icons_map.find("active:" + name());
    if (activeNamedIconIt != icons_map.end()) {
      return activeNamedIconIt->second;
    }

    auto activeIconIt = icons_map.find("active");
    if (activeIconIt != icons_map.end()) {
      return activeIconIt->second;
    }
  }

  if (isSpecial()) {
    auto specialNamedIconIt = icons_map.find("special:" + name());
    if (specialNamedIconIt != icons_map.end()) {
      return specialNamedIconIt->second;
    }

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

void Workspace::update(const std::string& workspace_icon) {
  if (this->m_workspaceManager.persistentOnly() && !this->isPersistent()) {
    m_button.hide();
    return;
  }
  // clang-format off
  if (this->m_workspaceManager.hideActive() && \
      this->isActive() && \
      !this->isPersistent() && \
      !this->isSpecial()) {
    // clang-format on
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
  addOrRemoveClass(styleContext, isSpecial(), name());
  addOrRemoveClass(styleContext, isEmpty(), "empty");
  addOrRemoveClass(styleContext, isPersistent(), "persistent");
  addOrRemoveClass(styleContext, isUrgent(), "urgent");
  addOrRemoveClass(styleContext, isVisible(), "visible");
  addOrRemoveClass(styleContext, m_workspaceManager.getBarOutput() == output(), "hosting-monitor");

  // Add workspace name as CSS class for per-workspace styling
  if (!m_prevNameClass.empty()) {
    styleContext->remove_class(m_prevNameClass);
    m_prevNameClass.clear();
  }
  auto nameClass = sanitizeCssClass(name());
  if (!nameClass.empty()) {
    styleContext->add_class(nameClass);
    m_prevNameClass = nameClass;
  }

  std::string windows;
  // Optimization: The {windows} substitution string is only possible if the taskbar is disabled, no
  // need to compute this if enableTaskbar() is true
  if (!m_workspaceManager.enableTaskbar()) {
    auto windowSeparator = m_workspaceManager.getWindowSeparator();
    auto groupThreshold = m_workspaceManager.windowRewriteGroupThreshold();

    auto end_it = m_workspaceManager.maxWindows() == 0
                      ? m_windowMap.end()
                      : m_windowMap.begin() + m_workspaceManager.maxWindows();

    if (groupThreshold > 0) {
      // Build ordered counts of each unique icon (including singular ones when threshold set to 1)
      std::vector<std::pair<std::string, int>> iconCounts;
      for (auto it = m_windowMap.begin(); it != end_it; ++it) {
        const auto& window_repr = *it;
        auto found = std::ranges::find_if(
            iconCounts, [&](const auto& p) { return p.first == window_repr.repr_rewrite; });
        if (found != iconCounts.end()) {
          found->second++;
        } else {
          iconCounts.emplace_back(window_repr.repr_rewrite, 1);
        }
      }

      // Format the group string
      auto groupFormat = m_workspaceManager.getWindowRewriteGroupFormat();
      bool isNotFirst = false;
      for (const auto& [icon, count] : iconCounts) {
        if (count >= groupThreshold) {
          if (isNotFirst) windows.append(windowSeparator);
          isNotFirst = true;
          try {
            windows.append(fmt::format(fmt::runtime(groupFormat), fmt::arg("icon", icon),
                                       fmt::arg("count", count)));
          } catch (const fmt::format_error& e) {
            spdlog::warn("Formatting window-rewrite-group-format error: {}", e.what());
            windows.append(icon);
          }
        } else {
          for (int i = 0; i < count; ++i) {
            if (isNotFirst) windows.append(windowSeparator);
            isNotFirst = true;
            windows.append(icon);
          }
        }
      }
    } else {
      // Not grouping icons
      bool isNotFirst = false;
      for (auto it = m_windowMap.begin(); it != end_it; ++it) {
        if (isNotFirst) windows.append(windowSeparator);
        isNotFirst = true;
        windows.append(it->repr_rewrite);
      }
    }
  }

  auto formatBefore = m_workspaceManager.formatBefore();
  m_labelBefore.set_markup(fmt::format(fmt::runtime(formatBefore), fmt::arg("id", id()),
                                       fmt::arg("name", name()), fmt::arg("icon", workspace_icon),
                                       fmt::arg("windows", windows)));
  m_labelBefore.get_style_context()->add_class("workspace-label");

  if (m_workspaceManager.enableTaskbar()) {
    updateTaskbar(workspace_icon);
  }
}

bool Workspace::isEmpty() const {
  auto ignore_list = m_workspaceManager.getIgnoredWindows();
  const bool no_ignore_rules = ignore_list.empty();

  if (no_ignore_rules) {
    return m_windows == 0;
  }
  // If there are windows but they are all ignored, consider the workspace empty
  return std::all_of(
      m_windowMap.begin(), m_windowMap.end(),
      [this, &ignore_list](const auto& window_repr) { return shouldSkipWindow(window_repr); });
}

void Workspace::updateTaskbar(const std::string& workspace_icon) {
  for (auto child : m_content.get_children()) {
    if (child != &m_labelBefore) {
      m_content.remove(*child);
    }
  }

  bool isFirst = true;
  auto processWindow = [&](const WindowRepr& window_repr) {
    if (shouldSkipWindow(window_repr)) {
      return;  // skip
    }
    if (isFirst) {
      isFirst = false;
    } else if (m_workspaceManager.getWindowSeparator() != "") {
      auto windowSeparator = Gtk::make_managed<Gtk::Label>(m_workspaceManager.getWindowSeparator());
      m_content.pack_start(*windowSeparator, false, false);
      windowSeparator->show();
    }

    auto window_box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_HORIZONTAL);
    window_box->set_tooltip_markup(window_repr.window_title);

    auto button = Gtk::manage(new Gtk::Button());
    button->set_relief(Gtk::RELIEF_NONE);
    button->add(*window_box);
    button->get_style_context()->add_class("taskbar-window");
    if (window_repr.isActive) {
      button->get_style_context()->add_class("active");
    }
    if (m_workspaceManager.onClickWindow() != "") {
      button->signal_button_press_event().connect(
          sigc::bind(sigc::mem_fun(*this, &Workspace::handleClick), window_repr.address), false);
    }

    auto text_before = fmt::format(fmt::runtime(m_workspaceManager.taskbarFormatBefore()),
                                   fmt::arg("title", window_repr.window_title));
    if (!text_before.empty()) {
      auto window_label_before = Gtk::make_managed<Gtk::Label>(text_before);
      window_box->pack_start(*window_label_before, true, true);
    }

    if (m_workspaceManager.taskbarWithIcon()) {
      auto app_info_ = IconLoader::get_app_info_from_app_id_list(window_repr.window_class);
      int icon_size = m_workspaceManager.taskbarIconSize();
      auto window_icon = Gtk::make_managed<Gtk::Image>();
      m_workspaceManager.iconLoader().image_load_icon(*window_icon, app_info_, icon_size);
      window_box->pack_start(*window_icon, false, false);
    }

    auto text_after = fmt::format(fmt::runtime(m_workspaceManager.taskbarFormatAfter()),
                                  fmt::arg("title", window_repr.window_title));
    if (!text_after.empty()) {
      auto window_label_after = Gtk::make_managed<Gtk::Label>(text_after);
      window_box->pack_start(*window_label_after, true, true);
    }

    m_content.pack_start(*button, true, false);
    button->show_all();
  };

  if (m_workspaceManager.taskbarReverseDirection()) {
    auto rend_it = m_workspaceManager.maxWindows() == 0
                       ? m_windowMap.rend()
                       : m_windowMap.rbegin() + m_workspaceManager.maxWindows();

    for (auto it = m_windowMap.rbegin(); it != rend_it; ++it) {
      processWindow(*it);
    }
  } else {
    auto end_it = m_workspaceManager.maxWindows() == 0
                      ? m_windowMap.end()
                      : m_windowMap.begin() + m_workspaceManager.maxWindows();

    for (auto it = m_windowMap.begin(); it != end_it; ++it) {
      processWindow(*it);
    }
  }

  auto formatAfter = m_workspaceManager.formatAfter();
  const bool has_format_after = !formatAfter.empty();

  if (has_format_after) {
    m_labelAfter.set_markup(fmt::format(fmt::runtime(formatAfter), fmt::arg("id", id()),
                                        fmt::arg("name", name()),
                                        fmt::arg("icon", workspace_icon)));
    m_content.pack_end(m_labelAfter, false, false);
    m_labelAfter.show();
  }
}

bool Workspace::handleClick(const GdkEventButton* event_button, WindowAddress const& addr) const {
  if (event_button->type == GDK_BUTTON_PRESS) {
    std::string command = std::regex_replace(m_workspaceManager.onClickWindow(),
                                             std::regex("\\{address\\}"), "0x" + addr);
    command = std::regex_replace(command, std::regex("\\{button\\}"),
                                 std::to_string(event_button->button));
    auto res = util::command::execNoRead(command);
    if (res.exit_code != 0) {
      spdlog::error("Failed to execute {}: {}", command, res.out);
    }
  }
  return true;
}

bool Workspace::shouldSkipWindow(const WindowRepr& window_repr) const {
  auto ignore_list = m_workspaceManager.getIgnoredWindows();
  auto it = std::ranges::find_if(ignore_list, [&window_repr](const auto& ignoreItem) {
    return std::regex_match(window_repr.window_class, ignoreItem) ||
           std::regex_match(window_repr.window_title, ignoreItem);
  });
  return it != ignore_list.end();
}

}  // namespace waybar::modules::hyprland
