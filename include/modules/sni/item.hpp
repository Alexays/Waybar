#pragma once

#include <dbus-status-notifier-item.h>
#include <giomm/dbusproxy.h>
#include <glibmm/refptr.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/image.h>
#include <gtkmm/menu.h>
#include <json/json.h>
#include <libdbusmenu-gtk/dbusmenu-gtk.h>
#include <sigc++/trackable.h>
#ifdef FILESYSTEM_EXPERIMENTAL
#include <experimental/filesystem>
#else
#include <filesystem>
#endif

namespace waybar::modules::SNI {

class Item : public sigc::trackable {
 public:
  Item(const std::string&, const std::string&, const Json::Value&);
  ~Item() = default;

  std::string bus_name;
  std::string object_path;

  int           icon_size;
  int           effective_icon_size;
  Gtk::Image    image;
  Gtk::EventBox event_box;
  std::string   category;
  std::string   id;
  std::string   status;

  std::string                  title;
  int32_t                      window_id;
  std::string                  icon_name;
  Glib::RefPtr<Gdk::Pixbuf>    icon_pixmap;
  Glib::RefPtr<Gtk::IconTheme> icon_theme;
  std::string                  overlay_icon_name;
  std::string                  attention_icon_name;
  std::string                  attention_movie_name;
  std::string                  icon_theme_path;
  std::string                  menu;
  DbusmenuGtkMenu*             dbus_menu = nullptr;
  Gtk::Menu*                   gtk_menu = nullptr;
  /**
   * ItemIsMenu flag means that the item only supports the context menu.
   * Default value is true because libappindicator supports neither ItemIsMenu nor Activate method
   * while compliant SNI implementation would always reset the flag to desired value.
   */
  bool item_is_menu = true;

 private:
  void proxyReady(Glib::RefPtr<Gio::AsyncResult>& result);
  void setProperty(const Glib::ustring& name, Glib::VariantBase& value);
  void getUpdatedProperties();
  void processUpdatedProperties(Glib::RefPtr<Gio::AsyncResult>& result);
  void onSignal(const Glib::ustring& sender_name, const Glib::ustring& signal_name,
                const Glib::VariantContainerBase& arguments);

  void                      updateImage();
  Glib::RefPtr<Gdk::Pixbuf> extractPixBuf(GVariant* variant);
  Glib::RefPtr<Gdk::Pixbuf> getIconByName(const std::string& name, int size);
  static void               onMenuDestroyed(Item* self, GObject* old_menu_pointer);
  void                      makeMenu();
  bool                      handleClick(GdkEventButton* const& /*ev*/);

  Glib::RefPtr<Gio::DBus::Proxy> proxy_;
  Glib::RefPtr<Gio::Cancellable> cancellable_;
  bool                           update_pending_;
};

}  // namespace waybar::modules::SNI
