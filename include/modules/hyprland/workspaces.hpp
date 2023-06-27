#include <gtkmm/button.h>
#include <gtkmm/label.h>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/hyprland/backend.hpp"

namespace waybar::modules::hyprland {

class Workspace {
 public:
  Workspace(int id);
  int id() { return id_; };
  Gtk::Button& button() { return button_; };

  static Workspace parse(const Json::Value&);
  void update();

 private:
  int id_;

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

  std::vector<Workspace> workspaces;

  std::mutex mutex_;
  const Bar& bar_;
  Gtk::Box box_;
};

}  // namespace waybar::modules::hyprland
