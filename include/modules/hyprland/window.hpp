#include <fmt/format.h>

#include "AAppIconLabel.hpp"
#include "bar.hpp"
#include "modules/hyprland/backend.hpp"
#include "util/json.hpp"

namespace waybar::modules::hyprland {

class Window : public waybar::AAppIconLabel, public EventHandler {
 public:
  Window(const std::string&, const waybar::Bar&, const Json::Value&);
  virtual ~Window();

  auto update() -> void override;

 private:
  struct Workspace {
    int id;
    int windows;
    std::string last_window;
    std::string last_window_title;

    static auto parse(const Json::Value&) -> Workspace;
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

  auto getActiveWorkspace(const std::string&) -> Workspace;
  auto getActiveWorkspace() -> Workspace;
  void onEvent(const std::string&) override;
  void queryActiveWorkspace();
  void setClass(const std::string&, bool enable);

  bool separate_outputs;
  std::mutex mutex_;
  const Bar& bar_;
  util::JsonParser parser_;
  WindowData window_data_;
  Workspace workspace_;
  std::string solo_class_;
  std::string last_solo_class_;
  bool solo_;
  bool all_floating_;
  bool swallowing_;
  bool fullscreen_;
};

}  // namespace waybar::modules::hyprland
