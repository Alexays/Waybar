#pragma once

#include <dbus-status-notifier-item.h>
#include <giomm/dbusproxy.h>
#include <giomm/menu.h>
#include <giomm/simpleaction.h>
#include <giomm/simpleactiongroup.h>
#include <glibmm/refptr.h>
#include <gtkmm/box.h>
#include <gtkmm/eventcontrollermotion.h>
#include <gtkmm/eventcontrollerscroll.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/image.h>
#include <gtkmm/popovermenu.h>
#include <json/json.h>
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

class Host;

using ItemOrderMap = std::unordered_map<std::string, int>;

class Item final : public sigc::trackable {
 public:
  Item(const std::string&, const std::string&, const Json::Value&, const Bar&,
       const std::function<void(Item&)>&, const std::function<void(Item&)>&,
       const std::function<void()>&, Host&, const ItemOrderMap&);
  ~Item();

  bool isReady() const;

  std::string bus_name;
  std::string object_path;
  Gtk::Box widget_;

  int icon_size;
  int effective_icon_size;
  Gtk::Image image;
  std::string category;
  std::string id;

  std::string title;
  std::string icon_name;
  Glib::RefPtr<Gdk::Pixbuf> icon_pixmap;
  bool has_custom_icon_{false};
  Glib::RefPtr<Gtk::IconTheme> icon_theme;
  std::string overlay_icon_name;
  Glib::RefPtr<Gdk::Pixbuf> overlay_icon_pixmap;
  std::string attention_icon_name;
  Glib::RefPtr<Gdk::Pixbuf> attention_icon_pixmap;
  std::string attention_movie_name;
  std::string icon_theme_path;
  std::string menu;
  ToolTip tooltip;
  /**
   * ItemIsMenu flag means that the item only supports the context menu.
   * Default value is true because libappindicator supports neither ItemIsMenu nor Activate method
   * while compliant SNI implementation would always reset the flag to desired value.
   */
  int order_ = -1;  // -1 means not set
  bool item_is_menu{true};

 private:
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

  void handleClick(int n_press, double x, double y);
  bool handleScroll(double dx, double dy);
  void handleMouseEnter(double x, double y);
  void handleMouseLeave();
  void setupDbusMenu();
  void onMenuProxyReady(Glib::RefPtr<Gio::AsyncResult>& result);
  void onMenuSignal(const Glib::ustring& sender, const Glib::ustring& signal_name,
                    const Glib::VariantContainerBase& args);
  void refreshMenuLayout();
  void onMenuLayoutReceived(Glib::RefPtr<Gio::AsyncResult>& result);
  void parseMenuLayout(const Glib::VariantBase& layout, const Glib::RefPtr<Gio::Menu>& target);
  void sendMenuEvent(int id);
  void showDbusMenu(double x, double y);

  // smooth scrolling threshold
  gdouble scroll_threshold_{0};
  gdouble distance_scrolled_x_{0};
  gdouble distance_scrolled_y_{0};
  // visibility of items with Status == Passive
  bool show_passive_{false};
  // hidden via config
  bool is_hidden_{false};
  bool ready_{false};
  Glib::ustring status_{"active"};

  const Bar& bar_;
  const std::function<void(Item&)> on_ready_;
  const std::function<void(Item&)> on_invalidate_;
  const std::function<void()> on_updated_;

  Glib::RefPtr<Gio::DBus::Proxy> proxy_;
  Glib::RefPtr<Gio::Cancellable> cancellable_;
  Glib::RefPtr<Gtk::GestureClick> gesture_click_;
  Glib::RefPtr<Gtk::EventControllerScroll> scroll_controller_;
  Glib::RefPtr<Gtk::EventControllerMotion> motion_controller_;
  std::set<std::string_view> update_pending_;

  Host& host_;
  const ItemOrderMap& orders_;

  // DBusMenu local menu (GTK4 replacement for Gtk::Menu)
  Glib::RefPtr<Gio::DBus::Proxy> menu_proxy_;
  Glib::RefPtr<Gio::Menu> menu_model_;
  Glib::RefPtr<Gio::SimpleActionGroup> menu_actions_;
  Gtk::PopoverMenu* popover_menu_{nullptr};
  bool menu_needs_rebuild_{true};
  bool menu_show_pending_{false};
  double menu_pending_x_{0};
  double menu_pending_y_{0};
};

}  // namespace waybar::modules::SNI
