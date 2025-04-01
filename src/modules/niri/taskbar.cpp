#include "modules/niri/taskbar.hpp"
#include <gio/gio.h>

#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <spdlog/spdlog.h>

namespace waybar::modules::niri {

Taskbar::Button::Button(const Json::Value &win, const Json::Value &cfg)
  : Button(win, cfg, Gtk::IconTheme::get_default())
{ }

Taskbar::Button::Button(const Json::Value &win, const Json::Value &cfg, const Glib::RefPtr<Gtk::IconTheme> &icon_theme)
  : gtk_button_contents_(Gtk::ORIENTATION_HORIZONTAL, 4),
    label_(""),
    icon_()
{
  this->pid_ = win["pid"].asUInt();
  this->gtk_button_contents_.add(this->icon_);
  this->gtk_button_contents_.add(this->label_);
  this->gtk_button.add(this->gtk_button_contents_);
  this->gtk_button.set_relief(Gtk::RELIEF_NONE);
  this->icon_theme_ = icon_theme;
  this->set_style(cfg);
  this->update(win);
}

Glib::RefPtr<Gdk::Pixbuf> Taskbar::Button::get_icon_from_app_id(std::string &app_id){
  auto icon_info = this->icon_theme_->lookup_icon(app_id, 24);
  // TODO(luna) Error handling to theme lookup?
  // TODO(luna) Icon sizing?
  return icon_info.load_icon();
}

void Taskbar::Button::update_icon() {
  auto pixbuf = this->get_icon_from_app_id(this->app_id_);
  auto scaled_icon_size = 24; // TODO(luna) Take this from config..

  if (pixbuf) {
    if (pixbuf->get_width() != scaled_icon_size) {
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
  this->app_id_ = app_id;
  this->label_.set_label(this->app_id_);
  this->update_icon();
}

bool Taskbar::Button::is_focused() {
  return this->gtk_button.get_style_context()->has_class("focused");
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
    spdlog::debug("Using fallback button format for app {}", this->app_id_);
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
}

void Taskbar::Button::update(const Json::Value &win) {
  auto app_id = win["app_id"].asString();
  const bool app_id_changed = this->app_id_ != app_id;
  if (app_id_changed) {
    this->update_app_id(app_id);
  }

  if (win["is_focused"].asBool()) {
    this->gtk_button.get_style_context()->add_class("focused");
  } else {
    this->gtk_button.get_style_context()->remove_class("focused");
  }

  this->show();
}

Taskbar::Taskbar(const std::string &id, const Bar &bar, const Json::Value &config)
    : AModule(config, "taskbar", id, false, false), bar_(bar), box_(bar.orientation, 0) {
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

void Taskbar::doUpdate() {
  auto ipcLock = gIPC->lockData();

  // Get my workspace id
  std::vector<Json::Value> my_workspaces;
  const auto &workspaces = gIPC->workspaces();
  auto my_workspace_id = 0;
  auto my_workspace_iter = std::ranges::find_if(
      workspaces,
      [&](const auto &ws) { // Get ws idx for active ws on same display as bar.
        bool ws_on_my_output = ws["output"].asString() == bar_.output->name;
        bool ws_is_active = ws["is_active"].asBool();
        return (ws_on_my_output && ws_is_active);
      });
  if (my_workspace_iter != std::ranges::end(workspaces)) {
    my_workspace_id = (*my_workspace_iter)["id"].asUInt();
  } else {
    spdlog::error("Failed to find workspace for current display output?");
    return;
  }
  spdlog::debug("Updating taskbar on output {} (workspace id {})", bar_.output->name, my_workspace_id);

  // Get windows in my workspace idx
  std::vector<Json::Value> my_windows;
  const auto &windows = gIPC->windows();
  std::ranges::copy_if(
      windows,
      std::back_inserter(my_windows),
      [&](const auto &win) {
        auto &win_wrk_id = win["workspace_id"];
        return (! win_wrk_id.isNull()) && (win_wrk_id.asInt() == my_workspace_id);
      });
  // Sort the windows vector by indicies.
  // XXX(LUNA) THIS IS BASED ON AN UNMERGED NIRI COMMIT. REVISIT THIS. FIELD NAMES WILL CHANGE.
  std::ranges::sort(
      my_windows,
      [](auto& a, auto& b){
        auto tile_pos_a = a["location"]["tile_pos_in_scrolling_layout"];
        auto tile_pos_b = b["location"]["tile_pos_in_scrolling_layout"];

        // XXX(luna) This could use geometric positions.
        // Handle windows that are NOT being tiled.
        if (tile_pos_a.isNull()) {
          return false;
        }
        if (tile_pos_b.isNull()) {
          return true;
        }

        auto &a_row = tile_pos_a[1];
        auto &a_col = tile_pos_a[0];
        auto &b_row = tile_pos_b[1];
        auto &b_col = tile_pos_b[0];

        if (a_col == b_col) {
          return a_row < b_row;
        }
        return a_col < b_col;
      });

  // Remove buttons for windows no longer on display (closed, moved, or ws changed).
  for (auto button_iter = buttons_.begin(); button_iter != buttons_.end();) {
    auto win_iter = std::ranges::find_if(
        my_windows,
        [button_iter](const auto &win) {
          return win["pid"].asUInt64() == button_iter->first;
        });

    if (win_iter == my_windows.end()) {
      spdlog::debug("{}: Remove Button for {}", bar_.output->name, button_iter->second.get_app_id());
      button_iter = buttons_.erase(button_iter);
    }
    else if ((*win_iter)["workspace_id"] != my_workspace_id) {
      spdlog::debug("{}: Remove Button for {}", bar_.output->name, button_iter->second.get_app_id());
      button_iter = buttons_.erase(button_iter);
    }
    else {
      button_iter++;
    }
  }

  // Update Buttons
  auto get_button = [this](const Json::Value &win) {
    auto button_iter = this->buttons_.find(win["pid"].asUInt());
    if (button_iter == this->buttons_.end()) { throw false; }
    return &button_iter->second;
  };
  for (const auto &win : my_windows) {
    Taskbar::Button *button;
    // Get button for window `win`.
    try {
      button = get_button(win);
      button->update(win);
      spdlog::debug("{}: Update Button for {}", bar_.output->name, button->get_app_id());
    }
    catch (...) {
      // Create a button for a new window.
      this->addButton(win);
      try {
        button = get_button(win);
        spdlog::debug("{}: Create Button for {}",bar_.output->name, button->get_app_id());
      }
      catch (...) {
        spdlog::error("{}: Failed to create a Button for pid {}", bar_.output->name, win["pid"]);
        continue; // Skip this broken window...
      }
    }

    auto style_context = button->gtk_button.get_style_context();

    if (win["is_focused"].asBool()) {
      style_context->add_class("active");
    }
    else {
      style_context->remove_class("active");
    }
  }

  // Refresh the button order.
  // This assumes that `my_windows` is ordered correctly.
  int pos = 0;
  for (auto& win : my_windows) {
    auto *button = get_button(win);
    box_.reorder_child(button->gtk_button, pos++);
  }
}

void Taskbar::update() {
  doUpdate();
  AModule::update();
}

void Taskbar::addButton(const Json::Value &win) {
  auto key = win["pid"].asUInt64();
  auto pair = buttons_.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(key),
    std::forward_as_tuple(win, this->config_, this->icon_theme_)
  );
  auto *button = &pair.first->second;
  box_.pack_start(button->gtk_button, false, false, 0);
}
}  // namespace waybar::modules::niri
