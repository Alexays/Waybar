#pragma once

#include <fmt/format.h>

#include <string>

#include "AAppIconLabel.hpp"
#include "bar.hpp"
#include "modules/hyprland/backend.hpp"

namespace waybar::modules::hyprland {

class WindowCount : public waybar::AAppIconLabel, public EventHandler {
 public:
  WindowCount(const std::string&, const waybar::Bar&, const Json::Value&,
              std::mutex&, std::list<pid_t>&);
  ~WindowCount() override;

  auto update() -> void override;

 private:
  struct Workspace {
    int id;
    int windows;
    bool hasfullscreen;
    static auto parse(const Json::Value& value) -> Workspace;
  };

  auto getActiveWorkspace(const std::string&) -> Workspace;
  auto getActiveWorkspace() -> Workspace;
  void onEvent(const std::string& ev) override;
  void queryActiveWorkspace();
  void setClass(const std::string&, bool enable);

  bool separateOutputs_;
  std::mutex mutex_;
  const Bar& bar_;
  Workspace workspace_;
  IPC& m_ipc;
};

}  // namespace waybar::modules::hyprland
