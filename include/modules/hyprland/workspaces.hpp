#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <memory>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/hyprland/backend.hpp"

namespace waybar::modules::hyprland {

struct WorkspaceDto {
  int id;

  static WorkspaceDto parse(const Json::Value& value);
};

class Workspace {
 public:
  Workspace(int id);
  Workspace(WorkspaceDto dto);
  int id() const { return id_; };
  int active() const { return active_; };
  std::string& select_icon(std::map<std::string, std::string>& icons_map);
  void set_active(bool value = true) { active_ = value; };
  Gtk::Button& button() { return button_; };

  void update(const std::string& format, const std::string& icon);

 private:
  int id_;
  bool active_;

  Gtk::Button button_;
  Gtk::Box content_;
  Gtk::Label label_;
};

class Workspaces : public AModule, public EventHandler {
 public:
  Workspaces(const std::string&, const waybar::Bar&, const Json::Value&);
  virtual ~Workspaces();
  void update() override;
  void init();

 private:
  void onEvent(const std::string&) override;
  void sort_workspaces();
  void create_workspace(int id);
  void remove_workspace(int id);

  std::string format_;
  std::map<std::string, std::string> icons_map_;
  bool with_icon_;
  int active_workspace_id;
  std::vector<std::unique_ptr<Workspace>> workspaces_;
  std::vector<int> workspaces_to_create_;
  std::vector<int> workspaces_to_remove_;
  std::mutex mutex_;
  const Bar& bar_;
  Gtk::Box box_;
};

}  // namespace waybar::modules::hyprland
