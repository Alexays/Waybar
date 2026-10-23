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
#include "util/gtk/gtk_icon.hpp"

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
    : m_workspaceManager_(workspace_manager),
      m_id_(workspace_data["id"].asInt()),
      m_name_(workspace_data["name"].asString()),
      m_output_(workspace_data["monitor"].asString()),  // TODO:allow using monitor desc
      m_windows_(workspace_data["windows"].asInt()),
      m_isActive_(true),
      m_isPersistentRule_(workspace_data["persistent-rule"].asBool()),
      m_isPersistentConfig_(workspace_data["persistent-config"].asBool()),
      m_ipc_(IPC::inst()) {
  if (m_name_.starts_with("name:")) {
    m_name_ = m_name_.substr(5);
  } else if (m_name_.starts_with("special")) {
    m_name_ = m_id_ == -99 ? m_name_ : m_name_.substr(8);
    m_isSpecial_ = true;
  }

  auto gesture_click{Gtk::GestureClick::create()};
  gesture_click->set_button(0);
  gesture_click->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  gesture_click->signal_pressed().connect(
      [this](int n_press, double x, double y) { handlePress(n_press, x, y); });
  m_button_.add_controller(gesture_click);

  auto controller_motion{Gtk::EventControllerMotion::create()};
  controller_motion->signal_enter().connect(sigc::mem_fun(*this, &Workspace::handleEnter));
  controller_motion->signal_leave().connect(sigc::mem_fun(*this, &Workspace::handleLeave));
  m_button_.add_controller(controller_motion);

  m_button_.add_css_class("flat");
  if (m_workspaceManager_.enableTaskbar()) {
    m_content_.set_orientation(m_workspaceManager_.taskbarOrientation());
  } else {
    m_labelBefore_.set_halign(Gtk::Align::CENTER);
    m_labelBefore_.set_hexpand(true);
  }
  m_content_.append(m_labelBefore_);
  m_button_.set_child(m_content_);

  initializeWindowMap(clients_data);
}

Workspace::~Workspace() {
  // Disconnect the hover-check timeout so it can't fire on this destroyed
  // instance (Workspaces are removed at runtime while a check may be armed).
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
  auto it = std::ranges::find_if(m_windowMap_,
                                 [&addr](const auto& window) { return window.address == addr; });
  // If the vector contains the address, remove it and return the window representation
  if (it != m_windowMap_.end()) {
    WindowRepr windowRepr = *it;
    m_windowMap_.erase(it);
    return windowRepr;
  }
  return std::nullopt;
}

void Workspace::handleEnter(double x, double y) {
  m_button_.get_style_context()->add_class("workspace-hover");
}

void Workspace::handleLeave() { m_button_.get_style_context()->remove_class("workspace-hover"); }

void Workspace::handlePress(int n_press, double x, double y) {
  try {
    if (id() > 0) {  // normal
      if (m_workspaceManager_.moveToMonitor()) {
        IPC::dispatch("focusworkspaceoncurrentmonitor", std::to_string(id()));
      } else {
        IPC::dispatch("workspace", std::to_string(id()));
      }
    } else if (!isSpecial()) {  // named (this includes persistent)
      if (m_workspaceManager_.moveToMonitor()) {
        IPC::dispatch("focusworkspaceoncurrentmonitor", "name:" + name());
      } else {
        IPC::dispatch("workspace", "name:" + name());
      }
    } else if (id() != -99) {  // named special
      IPC::dispatch("togglespecialworkspace", name());
    } else {  // special
      IPC::dispatch("togglespecialworkspace", "");
    }
    return;
  } catch (const std::exception& e) {
    spdlog::error("Failed to dispatch workspace: {}", e.what());
  }
  return;
}

void Workspace::initializeWindowMap(const Json::Value& clients_data) {
  m_windowMap_.clear();
  for (const auto& client : clients_data) {
    if (client["workspace"]["id"].asInt() == id()) {
      insertWindow({client});
    }
  }
}

void Workspace::setActiveWindow(WindowAddress const& addr) {
  std::optional<long> activeIdx;
  for (size_t i = 0; i < m_windowMap_.size(); ++i) {
    auto& window = m_windowMap_[i];
    bool isActive = (window.address == addr);
    window.setActive(isActive);
    if (isActive) {
      activeIdx = i;
    }
  }

  auto activeWindowPos = m_workspaceManager_.activeWindowPosition();
  const bool has_active_window =
      activeIdx.has_value() && activeWindowPos != Workspaces::ActiveWindowPosition::NONE;

  if (has_active_window) {
    auto window = std::move(m_windowMap_[*activeIdx]);
    m_windowMap_.erase(m_windowMap_.begin() + *activeIdx);
    if (activeWindowPos == Workspaces::ActiveWindowPosition::FIRST) {
      m_windowMap_.insert(m_windowMap_.begin(), std::move(window));
    } else if (activeWindowPos == Workspaces::ActiveWindowPosition::LAST) {
      m_windowMap_.emplace_back(std::move(window));
    }
  }
}

void Workspace::insertWindow(WindowCreationPayload create_window_payload) {
  if (!create_window_payload.isEmpty(m_workspaceManager_)) {
    auto repr = create_window_payload.repr(m_workspaceManager_);

    const bool should_display = !repr.empty() || m_workspaceManager_.enableTaskbar();

    if (should_display) {
      auto addr = create_window_payload.getAddress();
      auto it = std::ranges::find_if(
          m_windowMap_, [&addr](const auto& window) { return window.address == addr; });
      // If the vector contains the address, update the window representation, otherwise insert it
      if (it != m_windowMap_.end()) {
        *it = repr;
      } else if (!m_workspaceManager_.uniqueIcons() || repr.repr_rewrite.empty() ||
                 std::ranges::find_if(m_windowMap_, [&repr](const auto& window) {
                   return window.repr_rewrite == repr.repr_rewrite;
                 }) == m_windowMap_.end()) {
        m_windowMap_.emplace_back(repr);
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

  return m_name_;
}

void Workspace::doUpdate(const std::string& workspace_icon, const std::string& workspace_tooltip) {
  if (this->m_workspaceManager_.persistentOnly() && !this->isPersistent()) {
    m_button_.hide();
    return;
  }
  // clang-format off
  if (this->m_workspaceManager_.hideActive() && \
      this->isActive() && \
      !this->isPersistent() && \
      !this->isSpecial()) {
    // clang-format on
    m_button_.hide();
    return;
  }
  // clang-format off
  if (this->m_workspaceManager_.activeOnly() && \
     !this->isActive() && \
     !this->isPersistent() && \
     !this->isVisible() && \
     !this->isSpecial()) {
    // clang-format on
    // if activeOnly is true, hide if not active, persistent, visible or special
    m_button_.hide();
    return;
  }
  if (this->m_workspaceManager_.specialVisibleOnly() && this->isSpecial() && !this->isVisible()) {
    m_button_.hide();
    return;
  }
  m_button_.show();

  auto styleContext = m_button_.get_style_context();
  addOrRemoveClass(styleContext, isActive(), "active");
  addOrRemoveClass(styleContext, isSpecial(), "special");
  addOrRemoveClass(styleContext, isSpecial(), name());
  addOrRemoveClass(styleContext, isEmpty(), "empty");
  addOrRemoveClass(styleContext, isPersistent(), "persistent");
  addOrRemoveClass(styleContext, isUrgent(), "urgent");
  addOrRemoveClass(styleContext, isVisible(), "visible");
  addOrRemoveClass(styleContext, m_workspaceManager_.getBarOutput() == output(), "hosting-monitor");

  // Add workspace name as CSS class for per-workspace styling
  if (!m_prevNameClass_.empty()) {
    styleContext->remove_class(m_prevNameClass_);
    m_prevNameClass_.clear();
  }
  auto nameClass = sanitizeCssClass(name());
  if (!nameClass.empty()) {
    styleContext->add_class(nameClass);
    m_prevNameClass_ = nameClass;
  }

  std::string windows;
  // Optimization: The {windows} substitution string is only possible if the taskbar is disabled, no
  // need to compute this if enableTaskbar() is true
  if (!m_workspaceManager_.enableTaskbar()) {
    auto windowSeparator = m_workspaceManager_.getWindowSeparator();
    auto groupThreshold = m_workspaceManager_.windowRewriteGroupThreshold();

    auto end_it = (m_workspaceManager_.maxWindows() <= 0 ||
                   static_cast<size_t>(m_workspaceManager_.maxWindows()) >= m_windowMap_.size())
                      ? m_windowMap_.end()
                      : m_windowMap_.begin() + m_workspaceManager_.maxWindows();

    if (groupThreshold > 0) {
      // Build ordered counts of each unique icon (including singular ones when threshold set to 1)
      std::vector<std::pair<std::string, int>> iconCounts;
      for (auto it = m_windowMap_.begin(); it != end_it; ++it) {
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
      auto groupFormat = m_workspaceManager_.getWindowRewriteGroupFormat();
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
      for (auto it = m_windowMap_.begin(); it != end_it; ++it) {
        if (isNotFirst) windows.append(windowSeparator);
        isNotFirst = true;
        windows.append(it->repr_rewrite);
      }
    }
  }

  if (!workspace_tooltip.empty()) {
    m_button_.set_tooltip_text(
        fmt::format(fmt::runtime(workspace_tooltip), fmt::arg("id", id()), fmt::arg("name", name()),
                    fmt::arg("icon", workspace_icon), fmt::arg("windows", windows)));
  }

  auto formatBefore = m_workspaceManager_.formatBefore();
  m_labelBefore_.set_markup(fmt::format(fmt::runtime(formatBefore), fmt::arg("id", id()),
                                        fmt::arg("name", name()), fmt::arg("icon", workspace_icon),
                                        fmt::arg("windows", windows)));
  m_labelBefore_.get_style_context()->add_class("workspace-label");

  if (m_workspaceManager_.enableTaskbar()) {
    updateTaskbar(workspace_icon);
  }
}

bool Workspace::isEmpty() const {
  auto ignore_list = m_workspaceManager_.getIgnoredWindows();
  const bool no_ignore_rules = ignore_list.empty();

  if (no_ignore_rules) {
    return m_windows_ == 0;
  }
  // If there are windows but they are all ignored, consider the workspace empty
  return std::all_of(
      m_windowMap_.begin(), m_windowMap_.end(),
      [this, &ignore_list](const auto& window_repr) { return shouldSkipWindow(window_repr); });
}

void Workspace::updateTaskbar(const std::string& workspace_icon) {
  for (auto child : m_content_.get_children()) {
    if (child != &m_labelBefore_) {
      m_content_.remove(*child);
    }
  }

  // Build a list of windows to display, removing duplicates by window_class
  // and respecting max-icons limit
  std::vector<const WindowRepr*> windowsToShow;
  std::set<std::string> seenClasses;

  auto addWindowIfUnique = [&](const WindowRepr& window_repr) {
    if (shouldSkipWindow(window_repr)) {
      return;
    }
    // Deduplicate by window_class
    if (seenClasses.find(window_repr.window_class) != seenClasses.end()) {
      return;
    }
    seenClasses.insert(window_repr.window_class);
    windowsToShow.push_back(&window_repr);
  };

  if (m_workspaceManager_.taskbarReverseDirection()) {
    for (auto it = m_windowMap_.rbegin(); it != m_windowMap_.rend(); ++it) {
      addWindowIfUnique(*it);
    }
  } else {
    for (const auto& window_repr : m_windowMap_) {
      addWindowIfUnique(window_repr);
    }
  }

  // Apply max-icons limit if configured
  int maxIcons = m_workspaceManager_.taskbarMaxIcons();
  if (maxIcons > 0 && static_cast<int>(windowsToShow.size()) > maxIcons) {
    windowsToShow.resize(maxIcons);
  }

  // Apply max-windows limit if configured
  int maxWindows = m_workspaceManager_.maxWindows();
  if (maxWindows > 0 && static_cast<int>(windowsToShow.size()) > maxWindows) {
    windowsToShow.resize(maxWindows);
  }

  bool isFirst = true;
  for (const auto* window_repr : windowsToShow) {
    if (isFirst) {
      isFirst = false;
    } else if (m_workspaceManager_.getWindowSeparator() != "") {
      auto windowSeparator =
          Gtk::make_managed<Gtk::Label>(m_workspaceManager_.getWindowSeparator());
      m_content_.append(*windowSeparator);
      windowSeparator->show();
    }

    auto window_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    window_box->set_tooltip_markup(window_repr->window_title);

    auto button = Gtk::manage(new Gtk::Button());
    button->add_css_class("flat");
    button->set_child(*window_box);
    button->get_style_context()->add_class("taskbar-window");
    if (window_repr->isActive) {
      button->get_style_context()->add_class("active");
    }
    if (m_workspaceManager_.onClickWindow() != "") {
      auto gesture_click{Gtk::GestureClick::create()};
      gesture_click->set_button(0);
      gesture_click->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
      gesture_click->signal_pressed().connect(
          [this, gesture_click, window_repr](int n_press, double x, double y) {
            handlePress(n_press, x, y, gesture_click->get_current_event()->get_button(),
                        window_repr->address);
          });
      button->add_controller(gesture_click);
    }

    auto text_before = fmt::format(fmt::runtime(m_workspaceManager_.taskbarFormatBefore()),
                                   fmt::arg("title", window_repr->window_title));
    if (!text_before.empty()) {
      auto window_label_before = Gtk::make_managed<Gtk::Label>(text_before);
      window_label_before->set_hexpand(true);
      window_box->append(*window_label_before);
    }

    if (m_workspaceManager_.taskbarWithIcon()) {
      int icon_size{m_workspaceManager_.taskbarIconSize()};
      auto* icon{Gtk::make_managed<Gtk::Image>()};
      const auto app_info{util::gtk::HIcon::get_app_info_by_list(window_repr->window_class)};
      util::gtk::HIcon::image_load_icon(*icon, app_info, icon_size);
      window_box->append(*icon);
    }

    auto text_after = fmt::format(fmt::runtime(m_workspaceManager_.taskbarFormatAfter()),
                                  fmt::arg("title", window_repr->window_title));
    if (!text_after.empty()) {
      auto window_label_after = Gtk::make_managed<Gtk::Label>(text_after);
      window_label_after->set_hexpand(true);
      window_box->append(*window_label_after);
    }

    button->set_hexpand(true);
    button->set_halign(Gtk::Align::CENTER);
    m_content_.append(*button);
    button->show();
  }

  auto formatAfter = m_workspaceManager_.formatAfter();
  const bool has_format_after = !formatAfter.empty();

  if (has_format_after) {
    m_labelAfter_.set_markup(fmt::format(fmt::runtime(formatAfter), fmt::arg("id", id()),
                                         fmt::arg("name", name()),
                                         fmt::arg("icon", workspace_icon)));
    m_content_.append(m_labelAfter_);
    m_labelAfter_.show();
  }
}

void Workspace::handlePress(int n_press, double x, double y, guint button,
                            WindowAddress const& addr) {
  std::string command = std::regex_replace(m_workspaceManager_.onClickWindow(),
                                           std::regex("\\{address\\}"), "0x" + addr);
  command = std::regex_replace(command, std::regex("\\{button\\}"), std::to_string(button));
  auto res = util::command::execNoRead(command);
  if (res.exit_code != 0) {
    spdlog::error("Failed to execute {}: {}", command, res.out);
  }
}

bool Workspace::shouldSkipWindow(const WindowRepr& window_repr) const {
  auto ignore_list = m_workspaceManager_.getIgnoredWindows();
  auto it = std::ranges::find_if(ignore_list, [&window_repr](const auto& ignoreItem) {
    return std::regex_match(window_repr.window_class, ignoreItem) ||
           std::regex_match(window_repr.window_title, ignoreItem);
  });
  return it != ignore_list.end();
}

}  // namespace waybar::modules::hyprland
