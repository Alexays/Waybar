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
    enum class ButtonFormat : std::uint8_t {
      AppId,
      Title,
      Icon,
      IconAndAppId,
      IconAndTitle,
    };
    uint niri_id_;
    uint pid_;
    uint tile_pos_col;
    uint tile_pos_row;
    bool is_focused_;
    bool is_tiled_;
    uint icon_size_;
    std::string app_id_;
    std::string title_;
    ButtonFormat active_button_format_;
    ButtonFormat inactive_button_format_;
    Glib::RefPtr<Gtk::IconTheme> icon_theme_;
    Gtk::Box gtk_button_contents_;
    Gtk::Label label_;
    Gtk::Image icon_;
    Glib::RefPtr<Gdk::Pixbuf> get_icon_from_app_id(std::string &app_id);
    void update_icon();
    void update_text_label();
    void update_app_id(std::string &app_id);
    void update_title(std::string &title);
    bool is_floating();
   public:
    Gtk::Button gtk_button;
    Button(const Json::Value &window, const Json::Value &cfg, const Glib::RefPtr<Gtk::IconTheme> &icon_theme);
    Button(const Json::Value &window, const Json::Value &cfg);
    void show();
    void hide();
    void set_style(const Json::Value &cfg);
    bool update(const Json::Value &window);
    void destroy();
    std::string get_app_id() { return this->app_id_; };
    bool is_focused() const { return this->is_focused_; };
    uint get_niri_id() const { return this->niri_id_; };
    bool cmp(const Button &that) const;
  };

  class Workspace {
   private:
    enum class WorkspaceFormat : std::uint8_t {
      Default,
      LabelIdx,
      LabelWsName,
    };
    uint id_;
    uint idx_;
    bool is_active_;
    bool is_focused_;
    WorkspaceFormat active_workspace_format_;
    WorkspaceFormat inactive_workspace_format_;
    Json::Value config_;
    std::string name_;
    std::vector<Button> buttons_;
    Gtk::Button empty_workspace_btn_;
    Gtk::Label label_;
    void update_button_order();
    Glib::RefPtr<Gtk::IconTheme> icon_theme_;
    Taskbar::Button* update_button(const Json::Value &win);
    void set_style(const Json::Value &cfg);
   public:
    Workspace(const Json::Value &ws, const Json::Value &config, const Glib::RefPtr<Gtk::IconTheme> &icon_theme);
    Gtk::Box gtk_box;
    Gtk::Box gtk_box_buttons;
    uint get_id() const { return this->id_; };
    uint get_idx() const { return this->idx_; };
    bool update(const Json::Value &ws);
    void show();
    void hide() { this->gtk_box.hide(); };
    bool update_buttons(const std::vector<Json::Value> &windows);
    bool is_empty() { return this->buttons_.size() == 0; };
    bool is_active() { return std::ranges::any_of(this->buttons_, [](auto &btn){ return btn.is_focused(); }); };
    Button &get_button(uint id);
    Button pop_button(uint id);
  };

  std::vector<Json::Value> prev_windows_;
  std::vector<Json::Value> prev_workspaces_;
  const Bar &bar_;
  Gtk::Box box_;
  std::vector<Gtk::Separator> separators_;
  std::vector<Taskbar::Workspace> workspaces_;
  Glib::RefPtr<Gtk::IconTheme> icon_theme_;

  uint get_my_workspace_id();
  void update_workspaces();
  std::vector<Json::Value> get_workspaces_on_output();
  Gtk::Separator &get_separator(uint idx);
  void clean_separators(uint idx);

  void onEvent(const Json::Value &ev) override;
  void do_update();
};

}  // namespace waybar::modules::niri
