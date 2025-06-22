#pragma once

#include <fmt/format.h>

#include <string>

#include "AAppIconLabel.hpp"
#include "bar.hpp"
#include "modules/hyprland/backend.hpp"
#include "util/json.hpp"

namespace waybar::modules::hyprland {

class Window : public waybar::AAppIconLabel, public EventHandler {
 public:
  Window(const std::string&, const waybar::Bar&, const Json::Value&);
  ~Window() override;

  auto update() -> void override;

 private:
  struct Workspace {
    int id;
    int windows;
    std::string last_window;
    std::string last_window_title;

    static auto parse(const Json::Value& value) -> Workspace;
  };

  struct WindowData {
    bool floating;
    int monitor = -1;
    std::string class_name;
    std::string initial_class_name;
    std::string title;
    std::string initial_title;
    bool fullscreen;
    bool grouped;

    static auto parse(const Json::Value&) -> WindowData;
  };

  static auto getActiveWorkspace(const std::string&) -> Workspace;
  static auto getActiveWorkspace() -> Workspace;
  void onEvent(const std::string& ev) override;
  void queryActiveWorkspace();
  void setClass(const std::string&, bool enable);

  bool separateOutputs_;
  std::mutex mutex_;
  const Bar& bar_;
  util::JsonParser parser_;
  WindowData windowData_;
  Workspace workspace_;
  std::string soloClass_;
  std::string lastSoloClass_;
  bool solo_;
  bool allFloating_;
  bool swallowing_;
  bool fullscreen_;
  bool focused_;

  IPC& m_ipc;
};

}  // namespace waybar::modules::hyprland
