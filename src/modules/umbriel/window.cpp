#include "modules/umbriel/window.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <string>

#include "util/css_class.hpp"
#include "util/rewrite_string.hpp"
#include "util/sanitize_str.hpp"

namespace waybar::modules::umbriel {

Window::Window(const std::string& id, const Bar& bar, const Json::Value& config)
    : AAppIconLabel(config, "window", id, "{title}", 0, true), bar_(bar) {
  if (!gIPC) gIPC = std::make_unique<IPC>();

  gIPC->registerForIPC("windows", this);
  gIPC->registerForIPC("workspaces", this);
}

Window::~Window() { gIPC->unregisterForIPC(this); }

void Window::onEvent(const Json::Value& /*ev*/) { dp.emit(); }

void Window::doUpdate() {
  auto ipcLock = gIPC->lockData();

  const auto& windows = gIPC->windows();
  const auto& workspaces = gIPC->workspaces();

  const auto findWindow = [&](const std::string& id) -> const Json::Value* {
    if (id.empty()) return nullptr;
    const auto it = std::find_if(windows.cbegin(), windows.cend(),
                                 [&id](const auto& win) { return win["id"].asString() == id; });
    return it != windows.cend() ? &*it : nullptr;
  };

  const auto findWorkspace = [&](const std::string& id) -> const Json::Value* {
    if (id.empty()) return nullptr;
    const auto it = std::find_if(workspaces.cbegin(), workspaces.cend(),
                                 [&id](const auto& ws) { return ws["id"].asString() == id; });
    return it != workspaces.cend() ? &*it : nullptr;
  };

  const auto separate_outputs = config_["separate-outputs"].asBool();
  const Json::Value* window = nullptr;

  if (separate_outputs) {
    const auto ws_it = std::find_if(workspaces.cbegin(), workspaces.cend(), [&](const auto& ws) {
      return ws["active"].asBool() && ws["output"].asString() == bar_.output->name;
    });
    if (ws_it != workspaces.cend()) {
      window = findWindow((*ws_it)["focused_window"].asString());
    }
  } else {
    const auto active_it = std::find_if(windows.cbegin(), windows.cend(),
                                        [](const auto& win) { return win["active"].asBool(); });
    if (active_it != windows.cend()) {
      window = &*active_it;
    } else {
      // While a layer surface or the overview holds the keyboard no window is
      // seat-activated. Fall back to the remembered focus of the active
      // workspace, matching Umbriel's focused-window query.
      const auto ws_it = std::find_if(workspaces.cbegin(), workspaces.cend(),
                                      [](const auto& ws) { return ws["focused"].asBool(); });
      if (ws_it != workspaces.cend()) {
        window = findWindow((*ws_it)["focused_window"].asString());
      }
    }
  }

  setClass("empty", window == nullptr);

  if (window != nullptr) {
    const auto id = window->operator[]("id").asString();
    const auto workspace_id = window->operator[]("workspace").asString();
    const auto title = window->operator[]("title").asString();
    const auto appId = window->operator[]("app_id").asString();
    const auto workspace = findWorkspace(workspace_id);

    int max_col = -1;
    const auto column = window->operator[]("column").asInt();
    for (const auto& win : windows) {
      if (win["workspace"].asString() != workspace_id) {
        continue;
      }
      max_col = std::max(max_col, win["column"].asInt());
    }

    const auto sanitizedTitle = waybar::util::sanitize_string(title);
    const auto sanitizedAppId = waybar::util::sanitize_string(appId);
    const auto workspaceName = workspace != nullptr ? workspace->operator[]("name").asString() : "";
    const auto workspaceLayout =
        workspace != nullptr ? workspace->operator[]("layout").asString() : "";

    label_.show();
    label_.set_markup(waybar::util::rewriteString(
        fmt::format(fmt::runtime(format_), fmt::arg("title", sanitizedTitle),
                    fmt::arg("app_id", sanitizedAppId), fmt::arg("id", id),
                    fmt::arg("workspace", workspaceName), fmt::arg("workspace_id", workspace_id),
                    fmt::arg("layout", workspaceLayout),
                    fmt::arg("output", window->operator[]("output").asString()),
                    fmt::arg("column", column), fmt::arg("row", window->operator[]("row").asInt()),
                    fmt::arg("max_col", max_col)),
        config_["rewrite"]));

    updateAppIconName(appId, "");

    if (tooltipEnabled()) setTooltipMarkup(sanitizedTitle);

    const auto isSolo = std::none_of(windows.cbegin(), windows.cend(), [&](const auto& win) {
      return win["id"].asString() != id && win["workspace"].asString() == workspace_id;
    });
    setClass("solo", isSolo);

    // oldAppId_ is only ever a class this module added, so nothing else is removed
    if (!isSolo || oldAppId_ != appId) {
      if (!oldAppId_.empty()) {
        setClass(oldAppId_, false);
        oldAppId_.clear();
      }
      if (isSolo && waybar::util::add_app_id_class(bar_.window.get_style_context(), appId)) {
        oldAppId_ = appId;
      }
    }
  } else {
    if (config_["show-empty"].asBool()) {
      label_.show();
      label_.set_markup(waybar::util::rewriteString(
          fmt::format(fmt::runtime(format_), fmt::arg("title", ""), fmt::arg("app_id", ""),
                      fmt::arg("id", ""), fmt::arg("workspace", ""), fmt::arg("workspace_id", ""),
                      fmt::arg("layout", ""), fmt::arg("output", ""), fmt::arg("column", -1),
                      fmt::arg("row", -1), fmt::arg("max_col", -1)),
          config_["rewrite"]));
    } else {
      label_.hide();
    }

    updateAppIconName("", "");
    setClass("solo", false);
    if (!oldAppId_.empty()) setClass(oldAppId_, false);
    oldAppId_.clear();
  }
}

void Window::update() {
  doUpdate();
  AAppIconLabel::update();
}

void Window::setClass(const std::string& className, bool enable) {
  auto styleContext = bar_.window.get_style_context();
  if (enable) {
    if (!styleContext->has_class(className)) {
      styleContext->add_class(className);
    }
  } else {
    styleContext->remove_class(className);
  }
}

}  // namespace waybar::modules::umbriel
