#include <glibmm/main.h>
#include <json/value.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "modules/hyprland/workspaces.hpp"
#include "util/command.hpp"
#include "util/icon_loader.hpp"
#include "util/regex_input.hpp"
#include "util/string.hpp"
#include "util/tooltip.hpp"

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
      m_identity(parseWorkspaceIdentity(workspace_data).value_or(WorkspaceIdentity{})),
      m_output(workspace_data["monitor"].asString()),  // TODO:allow using monitor desc
      m_windows(workspace_data["windows"].asInt()),
      m_isActive(true),
      m_isPersistentRule(workspace_data["persistent-rule"].asBool()),
      m_isPersistentConfig(workspace_data["persistent-config"].asBool()),
      m_ipc(IPC::inst()) {
  // Bubble phase: a taskbar window button below gets the press first and may claim it
  auto click = Gtk::GestureClick::create();
  click->set_button(0);
  click->signal_pressed().connect([this, click = click.get()](int, double, double) {
    // A primary press stays on the button even when unhandled, as GtkButton claims it
    if (handleClicked() || click->get_current_button() == GDK_BUTTON_PRIMARY) {
      click->set_state(Gtk::EventSequenceState::CLAIMED);
    }
  });
  m_button.add_controller(click);

  // Enter and leave track the button with its taskbar children included
  auto motion = Gtk::EventControllerMotion::create();
  motion->signal_enter().connect(
      [this](double, double) { m_button.add_css_class("workspace-hover"); });
  motion->signal_leave().connect([this] { m_button.remove_css_class("workspace-hover"); });
  m_button.add_controller(motion);

  m_button.set_has_frame(false);
  m_content.append(m_labelBefore);
  if (m_workspaceManager.enableTaskbar()) {
    m_content.set_orientation(m_workspaceManager.taskbarOrientation());
  } else {
    // A centered label at its natural size, like a GtkBox center widget
    m_labelBefore.set_hexpand(true);
    m_labelBefore.set_halign(Gtk::Align::CENTER);
  }
  // The expanding children must not make the module expand in the bar
  m_content.set_hexpand(false);
  m_content.set_vexpand(false);
  m_button.set_child(m_content);

  initializeWindowMap(clients_data);
}

void addOrRemoveClass(Gtk::Widget& widget, bool condition, const std::string& class_name) {
  if (condition) {
    widget.add_css_class(class_name);
  } else {
    widget.remove_css_class(class_name);
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

bool Workspace::handleClicked() const {
  try {
    if (kind() == WorkspaceKind::Numbered) {  // normal
      if (m_workspaceManager.moveToMonitor()) {
        IPC::dispatch("focusworkspaceoncurrentmonitor", address());
      } else {
        IPC::dispatch("workspace", address());
      }
    } else if (!isSpecial()) {  // named (this includes persistent)
      if (m_workspaceManager.moveToMonitor()) {
        IPC::dispatch("focusworkspaceoncurrentmonitor", "name:" + name());
      } else {
        IPC::dispatch("workspace", "name:" + name());
      }
    } else {  // special
      // `togglespecialworkspace <name>` re-adds the namespace, so the display
      // name is the right argument for every special workspace -- the generic
      // one included, which Hyprland calls `special:special` and displays as
      // `special`.
      IPC::dispatch("togglespecialworkspace", name());
    }
    return true;
  } catch (const std::exception& e) {
    spdlog::error("Failed to dispatch workspace: {}", e.what());
  }
  return false;
}

void Workspace::setAddress(std::string const& value) { m_identity.renumber(value); }

void Workspace::initializeWindowMap(const Json::Value& clients_data) {
  m_windowMap.clear();
  for (const auto& client : clients_data) {
    const auto clientWorkspace = parseWorkspaceIdentity(client["workspace"]);
    if (!clientWorkspace.has_value()) {
      // Logged at debug, not warn: this loop runs once per workspace, so a
      // malformed client would otherwise be reported once per workspace too.
      spdlog::debug("Client {} carries no workspace identity; not assigning it to a workspace",
                    client["address"].asString());
      continue;
    }
    if (clientWorkspace->address == address()) {
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
      } else if (!m_workspaceManager.uniqueIcons() || repr.repr_rewrite.empty() ||
                 std::ranges::find_if(m_windowMap, [&repr](const auto& window) {
                   return window.repr_rewrite == repr.repr_rewrite;
                 }) == m_windowMap.end()) {
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

std::string& Workspace::selectString(std::map<std::string, std::string>& icons_map) {
  spdlog::trace("Selecting string for workspace {}", name());
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

  return m_identity.name;
}

void Workspace::update(const std::string& workspace_icon, const std::string& workspace_tooltip) {
  if (this->m_workspaceManager.persistentOnly() && !this->isPersistent()) {
    m_button.set_visible(false);
    return;
  }
  // clang-format off
  if (this->m_workspaceManager.hideActive() && \
      this->isActive() && \
      !this->isPersistent() && \
      !this->isSpecial()) {
    // clang-format on
    m_button.set_visible(false);
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
    m_button.set_visible(false);
    return;
  }
  if (this->m_workspaceManager.specialVisibleOnly() && this->isSpecial() && !this->isVisible()) {
    m_button.set_visible(false);
    return;
  }
  m_button.set_visible(true);

  addOrRemoveClass(m_button, isActive(), "active");
  addOrRemoveClass(m_button, isSpecial(), "special");
  addOrRemoveClass(m_button, isSpecial(), name());
  addOrRemoveClass(m_button, isEmpty(), "empty");
  addOrRemoveClass(m_button, isPersistent(), "persistent");
  addOrRemoveClass(m_button, isUrgent(), "urgent");
  addOrRemoveClass(m_button, isVisible(), "visible");
  addOrRemoveClass(m_button, m_workspaceManager.getBarOutput() == output(), "hosting-monitor");

  // Add workspace name as CSS class for per-workspace styling
  if (!m_prevNameClass.empty()) {
    m_button.remove_css_class(m_prevNameClass);
    m_prevNameClass.clear();
  }
  auto nameClass = sanitizeCssClass(name());
  if (!nameClass.empty()) {
    m_button.add_css_class(nameClass);
    m_prevNameClass = nameClass;
  }

  std::string windows;
  // Optimization: The {windows} substitution string is only possible if the taskbar is disabled, no
  // need to compute this if enableTaskbar() is true
  if (!m_workspaceManager.enableTaskbar()) {
    auto windowSeparator = m_workspaceManager.getWindowSeparator();
    auto groupThreshold = m_workspaceManager.windowRewriteGroupThreshold();

    auto end_it = (m_workspaceManager.maxWindows() <= 0 ||
                   static_cast<size_t>(m_workspaceManager.maxWindows()) >= m_windowMap.size())
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

  // Also when empty: an empty tooltip label shows no tooltip, which clears the previous one
  util::set_tooltip_markup(
      m_button, fmt::format(fmt::runtime(workspace_tooltip),
                            fmt::arg("id", Glib::Markup::escape_text(address()).raw()),
                            fmt::arg("name", Glib::Markup::escape_text(name()).raw()),
                            fmt::arg("icon", workspace_icon), fmt::arg("windows", windows)));

  auto formatBefore = m_workspaceManager.formatBefore();
  m_labelBefore.set_markup(fmt::format(fmt::runtime(formatBefore), fmt::arg("id", address()),
                                       fmt::arg("name", name()), fmt::arg("icon", workspace_icon),
                                       fmt::arg("windows", windows)));
  m_labelBefore.add_css_class("workspace-label");

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
  // Removing a managed child deletes it
  for (auto* child = m_labelBefore.get_next_sibling(); child != nullptr;) {
    auto* next = child->get_next_sibling();
    m_content.remove(*child);
    child = next;
  }

  // Build a list of windows to display. Duplicate window classes are only
  // collapsed to make room for the max-icons limit, so with the limit unset
  // every window is shown.
  int maxIcons = m_workspaceManager.taskbarMaxIcons();
  const bool deduplicate = maxIcons > 0;

  std::vector<const WindowRepr*> windowsToShow;
  std::set<std::string> seenClasses;

  auto addWindow = [&](const WindowRepr& window_repr) {
    if (shouldSkipWindow(window_repr)) {
      return;
    }
    if (deduplicate && !seenClasses.insert(window_repr.window_class).second) {
      return;
    }
    windowsToShow.push_back(&window_repr);
  };

  if (m_workspaceManager.taskbarReverseDirection()) {
    for (auto it = m_windowMap.rbegin(); it != m_windowMap.rend(); ++it) {
      addWindow(*it);
    }
  } else {
    for (const auto& window_repr : m_windowMap) {
      addWindow(window_repr);
    }
  }

  // Apply max-icons limit if configured
  if (maxIcons > 0 && static_cast<int>(windowsToShow.size()) > maxIcons) {
    windowsToShow.resize(maxIcons);
  }

  // Apply max-windows limit if configured
  int maxWindows = m_workspaceManager.maxWindows();
  if (maxWindows > 0 && static_cast<int>(windowsToShow.size()) > maxWindows) {
    windowsToShow.resize(maxWindows);
  }

  bool isFirst = true;
  for (const auto* window_repr : windowsToShow) {
    if (isFirst) {
      isFirst = false;
    } else if (m_workspaceManager.getWindowSeparator() != "") {
      m_content.append(*Gtk::make_managed<Gtk::Label>(m_workspaceManager.getWindowSeparator()));
    }

    const std::string title = toSingleLine(window_repr->window_title);
    auto window_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    std::string tooltip = title;
    util::limit_tooltip_text(tooltip);
    window_box->set_tooltip_text(tooltip);

    auto button = Gtk::make_managed<Gtk::Button>();
    button->set_has_frame(false);
    button->set_child(*window_box);
    button->add_css_class("taskbar-window");
    if (window_repr->isActive) {
      button->add_css_class("active");
    }
    // The press stops here, before the workspace button: any button for on-click-window, else
    // the primary one GtkButton claims
    const bool click_window = !m_workspaceManager.onClickWindow().empty();
    auto click = Gtk::GestureClick::create();
    click->set_button(click_window ? 0 : GDK_BUTTON_PRIMARY);
    click->signal_pressed().connect([this, click = click.get(), click_window,
                                     addr = window_repr->address](int, double, double) {
      click->set_state(Gtk::EventSequenceState::CLAIMED);
      if (click_window) {
        handleClick(click->get_current_button(), addr);
      }
    });
    button->add_controller(click);

    auto text_before = fmt::format(fmt::runtime(m_workspaceManager.taskbarFormatBefore()),
                                   fmt::arg("title", title));
    if (!text_before.empty()) {
      auto window_label_before = Gtk::make_managed<Gtk::Label>(text_before);
      window_label_before->set_hexpand(true);
      window_box->append(*window_label_before);
    }

    if (m_workspaceManager.taskbarWithIcon()) {
      auto app_info_ = IconLoader::get_app_info_from_app_id_list(window_repr->window_class);
      int icon_size = m_workspaceManager.taskbarIconSize();
      auto window_icon = Gtk::make_managed<Gtk::Image>();
      m_workspaceManager.iconLoader().image_load_icon(*window_icon, app_info_, icon_size);
      window_box->append(*window_icon);
    }

    auto text_after = fmt::format(fmt::runtime(m_workspaceManager.taskbarFormatAfter()),
                                  fmt::arg("title", title));
    if (!text_after.empty()) {
      auto window_label_after = Gtk::make_managed<Gtk::Label>(text_after);
      window_label_after->set_hexpand(true);
      window_box->append(*window_label_after);
    }

    if (m_workspaceManager.taskbarOrientation() == Gtk::Orientation::HORIZONTAL) {
      button->set_hexpand(true);
      button->set_halign(Gtk::Align::CENTER);
    } else {
      button->set_vexpand(true);
      button->set_valign(Gtk::Align::CENTER);
    }
    m_content.append(*button);
  }

  auto formatAfter = m_workspaceManager.formatAfter();
  const bool has_format_after = !formatAfter.empty();

  if (has_format_after) {
    m_labelAfter.set_markup(fmt::format(fmt::runtime(formatAfter), fmt::arg("id", address()),
                                        fmt::arg("name", name()),
                                        fmt::arg("icon", workspace_icon)));
    // Packed at the end: with no expanding window button, the free space comes before it
    if (m_workspaceManager.taskbarOrientation() == Gtk::Orientation::HORIZONTAL) {
      m_labelAfter.set_hexpand(windowsToShow.empty());
      m_labelAfter.set_halign(Gtk::Align::END);
    } else {
      m_labelAfter.set_vexpand(windowsToShow.empty());
      m_labelAfter.set_valign(Gtk::Align::END);
    }
    m_content.append(m_labelAfter);
  }
}

void Workspace::handleClick(guint button, WindowAddress const& addr) const {
  std::string command = std::regex_replace(m_workspaceManager.onClickWindow(),
                                           std::regex("\\{address\\}"), "0x" + addr);
  command = std::regex_replace(command, std::regex("\\{button\\}"), std::to_string(button));
  auto res = util::command::execNoRead(command);
  if (res.exit_code != 0) {
    spdlog::error("Failed to execute {}: {}", command, res.out);
  }
}

bool Workspace::shouldSkipWindow(const WindowRepr& window_repr) const {
  auto ignore_list = m_workspaceManager.getIgnoredWindows();
  auto matches = [](const std::string& text, const std::regex& rule) {
    try {
      util::RegexInput input{text};
      return std::regex_match(input.begin(), input.end(), rule);
    } catch (const std::regex_error&) {
      return false;
    }
  };
  auto it = std::ranges::find_if(ignore_list, [&window_repr, &matches](const auto& ignoreItem) {
    return matches(window_repr.window_class, ignoreItem) ||
           matches(window_repr.window_title, ignoreItem);
  });
  return it != ignore_list.end();
}

}  // namespace waybar::modules::hyprland
