#pragma once

#include <dbus-status-notifier-item.h>
#include <gtkmm.h>

namespace waybar::modules::SNI {

class Item {
public:
  Item(std::string, std::string, Glib::Dispatcher *);

  std::string bus_name;
  std::string object_path;
  Gtk::EventBox event_box;

  int icon_size;
  int effective_icon_size;
  Gtk::Image *image;
  Gtk::Menu *gtk_menu = nullptr;
  std::string category;
  std::string id;
  std::string status;

  std::string title;
  int32_t window_id;
  std::string icon_name;
  Glib::RefPtr<Gdk::Pixbuf> icon_pixmap;
  std::string overlay_icon_name;
  std::string attention_icon_name;
  std::string attention_movie_name;
  std::string icon_theme_path;
  std::string menu;
  bool item_is_menu;

private:
  static void proxyReady(GObject *obj, GAsyncResult *res, gpointer data);
  static void getAll(GObject *obj, GAsyncResult *res, gpointer data);
  static void handleActivate(GObject *, GAsyncResult *, gpointer);
  static void handleSecondaryActivate(GObject *, GAsyncResult *, gpointer);

  void updateImage();
  Glib::RefPtr<Gdk::Pixbuf> extractPixBuf(GVariant *variant);
  Glib::RefPtr<Gdk::Pixbuf> getIconByName(std::string name, int size);
  bool handleClick(GdkEventButton *const & /*ev*/);

  Glib::Dispatcher *dp_;
  GCancellable *cancellable_ = nullptr;
  SnOrgKdeStatusNotifierItem *proxy_ = nullptr;
};

} // namespace waybar::modules::SNI
