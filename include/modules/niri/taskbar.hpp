#pragma once

#include <gtkmm/button.h>
#include <json/value.h>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/niri/backend.hpp"

namespace waybar::modules::niri {

class Taskbar : public AModule, public EventHandler {
 public:
  Taskbar(const std::string &, const Bar &, const Json::Value &);
  ~Taskbar() override;
  void update() override;

 private:
  class Button {
   private:
    enum class ButtonFormat {
      Text,
      Icon,
      IconAndText,
    };
    uint niri_id_;
    uint pid_;
    uint icon_size_;
    std::string app_id_; ButtonFormat active_button_format_;
    ButtonFormat inactive_button_format_;
    Glib::RefPtr<Gtk::IconTheme> icon_theme_;
    Gtk::Box gtk_button_contents_;
    Gtk::Label label_;
    Gtk::Image icon_;
    Glib::RefPtr<Gdk::Pixbuf> get_icon_from_app_id(std::string &app_id);
    void update_icon();
    void update_app_id(std::string &app_id);
    void show();
    void hide();
    void send_niri_ipc_focus() const ;
   public:
    Gtk::Button gtk_button;
    Button(const Json::Value &window, const Json::Value &cfg, const Glib::RefPtr<Gtk::IconTheme> &icon_theme);
    Button(const Json::Value &window, const Json::Value &cfg);
    bool is_focused();
    void set_style(const Json::Value &cfg);
    void update(const Json::Value &window);
    void destroy();
    std::string get_app_id() { return this->app_id_; };
  };
  uint get_my_workspace_id();
  std::vector<Json::Value> get_workspaces_on_output();

  void onEvent(const Json::Value &ev) override;
  void doUpdate();
  void addButton(const Json::Value &win);

  const Bar &bar_;
  Gtk::Box box_;
  // Map from niri workspace id to button.
  std::unordered_map<uint64_t, Taskbar::Button> buttons_;
  Glib::RefPtr<Gtk::IconTheme> icon_theme_;
};

}  // namespace waybar::modules::niri
