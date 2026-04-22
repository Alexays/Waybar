#pragma once

#include <dbus-status-notifier-item.h>
#include <giomm/dbusproxy.h>
#include <glibmm/refptr.h>
#include <gtkmm/button.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/image.h>
#include <gtkmm/menu.h>
#include <json/json.h>
#include <libdbusmenu-gtk/dbusmenu-gtk.h>
#include <sigc++/trackable.h>

#include <functional>
#include <set>
#include <string_view>

#include "bar.hpp"

namespace waybar::modules::SNI {

struct ToolTip {
  Glib::ustring icon_name;
  Glib::ustring text;
};

class Item : public sigc::trackable {
 public:
  Item(const std::string&, const std::string&, const Json::Value&, const Bar&,
       const std::function<void(Item&)>&, const std::function<void(Item&)>&,
       const std::function<void()>&);
  ~Item();

  bool isReady() const;

  std::string bus_name;
  std::string object_path;

  int icon_size;
  int effective_icon_size;
  Gtk::Image image;
  Gtk::Button button;
  Gtk::EventBox event_box;
  std::string category;
  std::string id;

  std::string title;
  std::string icon_name;
  Glib::RefPtr<Gdk::Pixbuf> icon_pixmap;
  Glib::RefPtr<Gtk::IconTheme> icon_theme;
  std::string overlay_icon_name;
  Glib::RefPtr<Gdk::Pixbuf> overlay_icon_pixmap;
  std::string attention_icon_name;
  Glib::RefPtr<Gdk::Pixbuf> attention_icon_pixmap;
  std::string attention_movie_name;
  std::string icon_theme_path;
  std::string menu;
  ToolTip tooltip;
  DbusmenuGtkMenu* dbus_menu = nullptr;
  Gtk::Menu* gtk_menu = nullptr;
  /**
   * ItemIsMenu flag means that the item only supports the context menu.
   * Default value is true because libappindicator supports neither ItemIsMenu nor Activate method
   * while compliant SNI implementation would always reset the flag to desired value.
   */
  bool item_is_menu = true;

 private:
  void onConfigure(GdkEventConfigure* ev);
  void proxyReady(Glib::RefPtr<Gio::AsyncResult>& result);
  void setProperty(const Glib::ustring& name, Glib::VariantBase& value);
  void setStatus(const Glib::ustring& value);
  void setReady();
  void invalidate();
  void setCustomIcon(const std::string& id);
  void getUpdatedProperties();
  void processUpdatedProperties(Glib::RefPtr<Gio::AsyncResult>& result);
  void onSignal(const Glib::ustring& sender_name, const Glib::ustring& signal_name,
                const Glib::VariantContainerBase& arguments);

  void updateImage();
  static Glib::RefPtr<Gdk::Pixbuf> extractPixBuf(GVariant* variant);
  Glib::RefPtr<Gdk::Pixbuf> getIconPixbuf();
  Glib::RefPtr<Gdk::Pixbuf> getAttentionIconPixbuf();
  Glib::RefPtr<Gdk::Pixbuf> getOverlayIconPixbuf();
  Glib::RefPtr<Gdk::Pixbuf> loadIconFromNameOrFile(const std::string& name, bool log_failure);
  static Glib::RefPtr<Gdk::Pixbuf> overlayPixbufs(const Glib::RefPtr<Gdk::Pixbuf>&,
                                                  const Glib::RefPtr<Gdk::Pixbuf>&);
  Glib::RefPtr<Gdk::Pixbuf> getIconByName(const std::string& name, int size);
  double getScaledIconSize();
  static void onMenuDestroyed(Item* self, GObject* old_menu_pointer);
  void makeMenu();
  void updateAccessibleName();
  bool handleClick(GdkEventButton* const& /*ev*/);
  bool handleScroll(GdkEventScroll* const&);
  bool handleMouseEnter(GdkEventCrossing* const&);
  bool handleMouseLeave(GdkEventCrossing* const&);

  // smooth scrolling threshold
  gdouble scroll_threshold_ = 0;
  gdouble distance_scrolled_x_ = 0;
  gdouble distance_scrolled_y_ = 0;
  // visibility of items with Status == Passive
  bool show_passive_ = false;
  bool ready_ = false;
  Glib::ustring status_ = "active";

  const Bar& bar_;
  const std::function<void(Item&)> on_ready_;
  const std::function<void(Item&)> on_invalidate_;
  const std::function<void()> on_updated_;

  Glib::RefPtr<Gio::DBus::Proxy> proxy_;
  Glib::RefPtr<Gio::Cancellable> cancellable_;
  std::set<std::string_view> update_pending_;
};

}  // namespace waybar::modules::SNI
