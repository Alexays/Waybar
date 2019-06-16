#pragma once

#include <fmt/format.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include "IModule.hpp"
#include "bar.hpp"
#include "client.hpp"
#include "modules/sway/ipc/client.hpp"
#include "util/json.hpp"
#include "util/sleeper_thread.hpp"
#include <algorithm>
#include <functional>

namespace waybar::modules::sway {

class TaskBar : public IModule, public sigc::trackable {
 public:
  TaskBar(const std::string&, const waybar::Bar&, const Json::Value&);
  ~TaskBar() = default;
  auto update() -> void;
       operator Gtk::Widget&();

 private:
  void onCmd(const struct Ipc::ipc_response&);
  void onEvent(const struct Ipc::ipc_response&);
  void worker();
  bool filterButtons();
  void updateButtons();
  struct Application {
    int         id;
    std::string display_name;
    std::string instance_name;
  };
  Gtk::Button& addButton(const Application&);

  void        onButtonReady(Gtk::Button&);
  std::string getIcon(const std::string&);
  const int   getCycleTasks(int current_focus, bool prev);
  bool        handleScroll(GdkEventScroll*);
  void        parseTree(const Json::Value&);
  void        parseWorkspaceTree(const std::string& workspace_name, const Json::Value&);
  void        parseWorkspaceTreeEntry(const std::string& workspace_name, const Json::Value&);
  void        parseCurrentWorkspace(const Json::Value&);

  struct WorkspaceMap {
    int                      focused_num = 0;
    std::vector<Application> applications;
  };

  const Bar&                           bar_;
  const Json::Value&                   config_;
  waybar::util::SleeperThread          thread_;
  std::mutex                           mutex_;
  Gtk::Box                             box_;
  Ipc                                  ipc_;
  util::JsonParser                     parser_;
  bool                                 scrolling_;
  std::unordered_map<int, Gtk::Button> buttons_;
  std::string                          current_workspace;
  std::map<std::string, WorkspaceMap>  taskMap_;
};

}  // namespace waybar::modules::sway
