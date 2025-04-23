#include "modules/niri/taskbar.hpp"
#include "util/gtk_icon.hpp"
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <spdlog/spdlog.h>
#include <cctype>

namespace waybar::modules::niri {

Taskbar::Button::Button(const Json::Value &win, const Json::Value &cfg)
  : Button(win, cfg, Gtk::IconTheme::get_default())
{ }

void send_niri_ipc_focus(uint id) {
  Json::Value request(Json::objectValue);
  auto &action = (request["Action"] = Json::Value(Json::objectValue));
  auto &focus_window = (action["FocusWindow"] = Json::Value(Json::objectValue));
  focus_window["id"] = id;
  IPC::send(request);
}

Taskbar::Button::Button(const Json::Value &win, const Json::Value &cfg, const Glib::RefPtr<Gtk::IconTheme> &icon_theme)
  : gtk_button_contents_(Gtk::ORIENTATION_HORIZONTAL, 4),
    label_(""),
    icon_()
{
  uint id = win["id"].asUInt();
  this->niri_id_ = id;
  spdlog::debug("Creating new button for app {} with id {}", win["app_id"].asString(), this->niri_id_);
  this->gtk_button_contents_.add(this->icon_);
  this->gtk_button_contents_.add(this->label_);
  this->gtk_button.add(this->gtk_button_contents_);
  this->gtk_button.set_relief(Gtk::RELIEF_NONE);
  this->gtk_button.signal_pressed().connect([id] {
      try { send_niri_ipc_focus(id); }
      catch (const std::exception &e) { spdlog::error("Error switching focus: {}", e.what()); }
    });
  this->icon_theme_ = icon_theme;
  this->icon_size_ = 24;
  this->set_style(cfg);
  this->update(win);
}

Glib::RefPtr<Gdk::Pixbuf> Taskbar::Button::get_icon_from_app_id(std::string &app_id){
  spdlog::debug("Attempting to load icon with app_id '{}'", app_id);
  auto icon_info = this->icon_theme_->lookup_icon(app_id, this->icon_size_);
  if (icon_info) {
    return icon_info.load_icon();
  }

  // Assume that app_id might be startup_wm from a desktop file, attempt a lookup.
  std::string real_app_id = "";
  gchar ***desktop_list = g_desktop_app_info_search(app_id.c_str());
  if (desktop_list != nullptr && desktop_list[0] != nullptr) {
    for (size_t i = 0; desktop_list[0][i] != nullptr; i++) {
      auto tmp_info = Gio::DesktopAppInfo::create(desktop_list[0][i]);
      // see https://github.com/Alexays/Waybar/issues/1446
      if (!tmp_info) { continue; }

      auto startup_class = tmp_info->get_startup_wm_class();
      auto cmp_ichar = [](auto a, auto b) {
        return std::tolower(static_cast<unsigned char>(a)) ==
           std::tolower(static_cast<unsigned char>(b));
      };
      if (std::ranges::equal(startup_class, app_id, cmp_ichar)) {
        real_app_id = tmp_info->get_string("Icon");
        break;
      }
    }
    g_strfreev(desktop_list[0]);
  }
  g_free(desktop_list);
  // Retry lookup with real_app_id
  spdlog::debug("Attempting to load icon with found app_id '{}'", real_app_id);
  icon_info = this->icon_theme_->lookup_icon(real_app_id, this->icon_size_);
  if (icon_info) {
    return icon_info.load_icon();
  }

  // Fallback icon
  return DefaultGtkIconThemeWrapper::load_icon(
    "image-missing",
    this->icon_size_,
    Gtk::IconLookupFlags::ICON_LOOKUP_FORCE_SIZE
  );
}

void Taskbar::Button::update_icon() {
  auto pixbuf = this->get_icon_from_app_id(this->app_id_);
  auto scaled_icon_size = this->icon_size_;

  if (pixbuf) {
    if ((unsigned)pixbuf->get_width() != scaled_icon_size) {
      int width = scaled_icon_size * pixbuf->get_width() / pixbuf->get_height();
      pixbuf = pixbuf->scale_simple(width, scaled_icon_size, Gdk::InterpType::INTERP_BILINEAR);
    }
    auto surface = Gdk::Cairo::create_surface_from_pixbuf(
        pixbuf,
        this->icon_.get_scale_factor(),
        this->icon_.get_window()
    );
    this->icon_.set(surface);
  }
}

void Taskbar::Button::show() {
  auto button_format = this->is_focused() ? this->active_button_format_ : inactive_button_format_;

  switch(button_format) {
    case ButtonFormat::Text:
      this->label_.show();
      this->icon_.hide();
      break;
    case ButtonFormat::Icon:
      this->label_.hide();
      this->icon_.show();
      break;
    case ButtonFormat::IconAndText:
      this->label_.show();
      this->icon_.show();
      break;
  }

  this->gtk_button_contents_.show();
  this->gtk_button.show();
}

void Taskbar::Button::hide() {
  this->label_.hide();
  this->icon_.hide();
  this->gtk_button_contents_.hide();
  this->gtk_button.hide();
}

void Taskbar::Button::update_app_id(std::string &app_id) {

  if (this->app_id_ == app_id) {
    return;
  }
  auto style = this->gtk_button.get_style_context();
  style->remove_class(this->app_id_);
  this->app_id_ = app_id;
  style->add_class(this->app_id_);
  this->label_.set_label(this->app_id_);
  this->update_icon();
}

void Taskbar::Button::set_style(const Json::Value &cfg) {
  auto format_to_enum = [this](const std::string &format_str) {
    if (format_str == "text") {
      return ButtonFormat::Text;
    }
    if (format_str == "icon") {
      return ButtonFormat::Icon;
    }
    if (format_str == "icon-and-text") {
      return ButtonFormat::IconAndText;
    }
    spdlog::debug("No valid button style provided for {}", this->app_id_);
    return ButtonFormat::Icon; // Default Fallback
  };

  auto warn_if_missing = [](const Json::Value &cfg, const std::string &key) {
    if (cfg.isNull()) {
      spdlog::debug("Taskbar missing config for '{}'");
    }
  };

  std::string cfg_key = "active-button-format";
  warn_if_missing(cfg, cfg_key);
  this->active_button_format_ = format_to_enum(cfg[cfg_key].asString());

  cfg_key = "inactive-button-format";
  warn_if_missing(cfg, cfg_key);
  this->inactive_button_format_ = format_to_enum(cfg[cfg_key].asString());

  cfg_key = "icon-size";
  if (!cfg[cfg_key].isNull()) {
    this->icon_size_ = cfg[cfg_key].asUInt();
  }
}

bool Taskbar::Button::update(const Json::Value &win) {
  // Update object from json
  if (win["id"].asUInt() != this->niri_id_) {
    return false;
  }
  this->pid_ = win["pid"].asUInt();
  this->is_focused_ = win["is_focused"].asBool();
  auto app_id = win["app_id"].asString();
  this->update_app_id(app_id);
  auto tile_pos = win["location"]["tile_pos_in_scrolling_layout"];
  this->is_tiled_ = !tile_pos.isNull();
  if (this->is_tiled_) {
    this->tile_pos_col = tile_pos[0].asUInt();
    this->tile_pos_row = tile_pos[1].asUInt();
  }

  // Update Style Ctx from Object Fields
  auto style_ctx = this->gtk_button.get_style_context();
  if (this->is_focused_) {
    style_ctx->add_class("focused");
    style_ctx->add_class("active");
  } else {
    style_ctx->remove_class("focused");
    style_ctx->remove_class("active");
  }

  this->show();
  return true;
}

bool Taskbar::Button::cmp(const Button &that) const {
  if (!this->is_tiled_) {
    return false;
  }
  if (!that.is_tiled_) {
    return true;
  }
  return this->tile_pos_col == that.tile_pos_col ?
      (this->tile_pos_row < that.tile_pos_row) :
      (this->tile_pos_col < that.tile_pos_col);
}


Taskbar::Workspace::Workspace(const Json::Value &ws, const Json::Value &config, const Glib::RefPtr<Gtk::IconTheme> &icon_theme)
  : buttons_(), gtk_box(Gtk::ORIENTATION_HORIZONTAL, 0)
{
  if (ws["id"].isNull()) {
    spdlog::error("Workspace contructor fed invalid workspace Json!");
    throw false;
  }
  auto style = this->gtk_box.get_style_context();
  style->add_class("workspace");
  this->gtk_box.set_name("workspace");
  this->icon_theme_ = icon_theme;
  this->id_ = ws["id"].asUInt();
  this->config_ = config;
  this->update(ws);
}

bool Taskbar::Workspace::update(const Json::Value &ws) {
  if (this->id_ != ws["id"].asUInt()) {
    return false;
  }
  this->idx_ = ws["idx"].asUInt();
  this->name_ = ws["name"].asString();

  return true;
}

Taskbar::Button* Taskbar::Workspace::update_button(const Json::Value &win) {
  bool btn_in_workspace = (this->id_ == win["workspace_id"].asUInt());
  Taskbar::Button *updated_button = nullptr;
  // Try to find button that matches given win.
  for (auto btn_it = this->buttons_.begin(); btn_it != this->buttons_.end(); btn_it++) {
    auto &btn = *btn_it;
    if (btn.update(win)) {
      updated_button = &btn;
      if (!btn_in_workspace) {
        // We matched the button, but its not in our workspace.. Destroy it.
        this->gtk_box.remove(btn.gtk_button);
        this->buttons_.erase(btn_it);
      }
      break;
    }
  }

  // Create button if needed
  if (btn_in_workspace && (updated_button == nullptr)) {
    auto &button = this->buttons_.emplace_back(win, this->config_, this->icon_theme_);
    updated_button = &button;
    this->gtk_box.pack_start(button.gtk_button, false, false, 0);
  }

  return updated_button;
}

bool Taskbar::Workspace::update_buttons(const std::vector<Json::Value> &windows) {
  std::vector<uint> updated_win_ids;

  // Update buttons
  for (const auto &win : windows) {
    auto *updated_button = this->update_button(win);
    if (updated_button != nullptr) {
      updated_win_ids.emplace_back(updated_button->get_niri_id());
    }
  }

  // Erase buttons for window ids that did NOT just update.
  auto new_end = std::ranges::remove_if(this->buttons_, [this, updated_win_ids](auto &btn){
    auto window_stale = !std::ranges::any_of(
        updated_win_ids,
        [&btn](uint win_id) { return win_id == btn.get_niri_id(); }
    );
    if (window_stale) {
      this->gtk_box.remove(btn.gtk_button);
    }
    return window_stale;
  }).begin();

  this->buttons_.erase(new_end, this->buttons_.end());

  this->update_button_order();

  // Update Style Ctx from Object Fields
  auto style_ctx = this->gtk_box.get_style_context();
  if (this->is_active()) {
    style_ctx->add_class("focused");
    style_ctx->add_class("active");
  } else {
    style_ctx->remove_class("focused");
    style_ctx->remove_class("active");
  }

  return true;
}

void Taskbar::Workspace::update_button_order() {
  std::ranges::sort(this->buttons_, [](auto& a, auto& b){ return a.cmp(b); });
  uint pos = 0;
  for (auto& btn : this->buttons_) {
    this->gtk_box.reorder_child(btn.gtk_button, pos++);
  }
}

Taskbar::Taskbar(const std::string &id, const Bar &bar, const Json::Value &config)
    : AModule(config, "taskbar", id, false, false), bar_(bar), box_(bar.orientation, 0)
{
  this->box_.set_name("taskbar");
  if (!id.empty()) {
    this->box_.get_style_context()->add_class(id);
  }
  this->box_.get_style_context()->add_class(MODULE_CLASS);
  this->event_box_.add(this->box_);
  this->icon_theme_ = Gtk::IconTheme::get_default();

  if (!gIPC) {
    gIPC = std::make_unique<IPC>();
  }
  gIPC->registerForIPC("WorkspacesChanged", this);
  gIPC->registerForIPC("WorkspaceActivated", this);
  gIPC->registerForIPC("WorkspaceActiveWindowChanged", this);

  gIPC->registerForIPC("WindowsChanged", this);
  gIPC->registerForIPC("WindowOpenedOrChanged", this);
  gIPC->registerForIPC("WindowsLocationsChanged", this);
  gIPC->registerForIPC("WindowFocusChanged", this);
  gIPC->registerForIPC("WindowClosed", this);

  this->dp.emit();
}

Taskbar::~Taskbar() { gIPC->unregisterForIPC(this); }

void Taskbar::onEvent(const Json::Value &ev) { this->dp.emit(); }

uint Taskbar::get_my_workspace_id() {
  const auto &workspaces = gIPC->workspaces();
  auto my_workspace_iter = std::ranges::find_if(
      workspaces,
      [this](const auto &ws) { // Get ws idx for active ws on same display as bar.
        bool ws_on_my_output = ws["output"].asString() == this->bar_.output->name;
        bool ws_is_active = ws["is_active"].asBool();
        return (ws_on_my_output && ws_is_active);
      });
  if (my_workspace_iter == std::ranges::end(workspaces)) {
    throw "Failed to find a niri workspace on bar display output";
  }
  return (*my_workspace_iter)["id"].asUInt();
}

std::vector<Json::Value> Taskbar::get_workspaces_on_output() {
  std::vector<Json::Value> my_workspaces;
  const auto &workspaces = gIPC->workspaces();
  std::ranges::copy_if(
      workspaces,
      std::back_inserter(my_workspaces),
      [this] (const auto &ws) {
        return ws["output"].asString() == this->bar_.output->name;
      }
  );
  return my_workspaces;
}

void Taskbar::update_workspaces() {
  auto ws_vec = this->get_workspaces_on_output();

  // Update Workspaces
  for (auto workspace_it = this->workspaces_.begin(); workspace_it != this->workspaces_.end(); ) {
    auto &workspace = *workspace_it;
    auto ws_it = ws_vec.begin();
    for (ws_it = ws_vec.begin(); ws_it != ws_vec.end() && !workspace.update(*ws_it); ws_it++) ;
    if (ws_it == ws_vec.end()) {
      // stale workspace
      this->box_.remove(workspace.gtk_box);
      this->workspaces_.erase(workspace_it);
    } else {
      ws_vec.erase(ws_it);
      workspace_it++;
    }
  };

  // ws_vec only contains ws json that did not match existing workspaces.. Make them!
  for (auto &ws : ws_vec) {
    auto &workspace = this->workspaces_.emplace_back(ws, this->config_, this->icon_theme_);
    this->box_.pack_start(workspace.gtk_box, false, false, 0);
  }

  // Reorder As Needed
  // Kinda awkward since seperator rules aren't enforced by objects, but by math here.
  // Effectively.. Evens indexes are workspaces, odds are separators.
  uint ws_last_idx = this->workspaces_.size();
  spdlog::info("Adding ws's of len {}", ws_last_idx);
  for (auto &workspace : this->workspaces_) {
    // niri workspaces indexes start at 1
    auto i = workspace.get_idx() - 1;
    this->box_.reorder_child(workspace.gtk_box, (i * 2) + 1);
    if (workspace.get_idx() != 1 && !workspace.is_empty()) {
      spdlog::info("giving it a sep.. {}", i);
      auto &sep = this->getSeparator(i);
      this->box_.reorder_child(sep, (i * 2));
      sep.show();
    }
  }
  this->cleanSeparators(ws_last_idx);
}


void Taskbar::doUpdate() {
  auto ipcLock = gIPC->lockData();
  auto my_workspace_id = this->get_my_workspace_id();
  spdlog::debug("Updating taskbar on output {} (workspace id {})", bar_.output->name, my_workspace_id);

  // TODO: This should only happen on events that provoke it.
  this->update_workspaces();

  // TODO: This should only happen on events that provoke it.
  const auto &windows = gIPC->windows();
  for (auto &workspace : this->workspaces_) {
    workspace.update_buttons(windows);
  }

  for (auto &workspace : this->workspaces_) {
    workspace.show();
  }
}

void Taskbar::update() {
  doUpdate();
  AModule::update();
}

Gtk::Separator &Taskbar::getSeparator(uint idx) {
  while (idx >= this->separators_.size()) {
    this->separators_.emplace_back();
  }
  auto &sep = this->separators_.at(idx);
  if (sep.get_parent() == nullptr){
    this->box_.pack_start(sep, false, false, 0);
  };
  return sep;
}

void Taskbar::cleanSeparators(uint idx) {
  if (idx > this->separators_.size()) {
    return;
  }
  this->separators_.erase(this->separators_.begin() + idx, this->separators_.end());
}
}  // namespace waybar::modules::niri
