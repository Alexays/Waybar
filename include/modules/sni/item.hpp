#pragma once

#include <dbus-status-notifier-item.h>
#include <giomm/dbusproxy.h>
#include <giomm/simpleactiongroup.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gtkmm/eventcontrollerscroll.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/popovermenu.h>
#include <json/json.h>
#include <libdbusmenu-glib/client.h>
#include <sigc++/trackable.h>

#include <functional>
#include <set>
#include <string_view>

#include "bar.hpp"
#include "util/module_box.hpp"
#include "util/scaled_image.hpp"

namespace waybar::modules::SNI {

struct ToolTip {
  Glib::ustring icon_name;
  Glib::ustring title;
  // Pango markup
  Glib::ustring text;
};

class Host;

using ItemOrderMap = std::unordered_map<std::string, int>;

class Item : public sigc::trackable {
 public:
  Item(const std::string&, const std::string&, const Json::Value&, const Bar&,
       const std::function<void(Item&)>&, const std::function<void(Item&)>&,
       const std::function<void()>&, Host&, const ItemOrderMap&);
  ~Item();

  bool isReady() const;

  std::string bus_name;
  std::string object_path;

  int icon_size;
  int effective_icon_size;
  util::ScaledImage image;
  util::ModuleBox event_box;
  std::string category;
  std::string id;

  std::string title;
  std::string icon_name;
  Glib::RefPtr<Gdk::Pixbuf> icon_pixmap;
  bool has_custom_icon_ = false;
  std::string overlay_icon_name;
  Glib::RefPtr<Gdk::Pixbuf> overlay_icon_pixmap;
  std::string attention_icon_name;
  Glib::RefPtr<Gdk::Pixbuf> attention_icon_pixmap;
  std::string attention_movie_name;
  std::string icon_theme_path;
  std::string menu;
  ToolTip tooltip;
  DbusmenuClient* dbus_menu = nullptr;
  Gtk::PopoverMenu* gtk_menu = nullptr;
  /**
   * ItemIsMenu flag means that the item only supports the context menu.
   * Default value is true because libappindicator supports neither ItemIsMenu nor Activate method
   * while compliant SNI implementation would always reset the flag to desired value.
   */
  bool item_is_menu = true;
  int order_ = -1;  // -1 means not set

 private:
  void proxyReady(Glib::RefPtr<Gio::AsyncResult>& result);
  void setProperty(const std::string& name, Glib::VariantBase& value);
  void setStatus(const Glib::ustring& value);
  void setTooltipMarkup(const Glib::ustring& markup);
  void setReady();
  void invalidate();
  void setCustomIcon(const std::string& id);
  void getUpdatedProperties();
  void processUpdatedProperties(Glib::RefPtr<Gio::AsyncResult>& result);
  void onSignal(const Glib::ustring& sender_name, const Glib::ustring& signal_name,
                const Glib::VariantContainerBase& arguments);

  void updateImage();
  Glib::RefPtr<Gdk::Pixbuf> extractPixBuf(GVariant* variant);
  Glib::RefPtr<Gdk::Pixbuf> getIconPixbuf();
  Glib::RefPtr<Gdk::Pixbuf> getAttentionIconPixbuf();
  Glib::RefPtr<Gdk::Pixbuf> getOverlayIconPixbuf();
  Glib::RefPtr<Gdk::Pixbuf> loadIconFromNameOrFile(const std::string& name, bool log_failure);
  static Glib::RefPtr<Gdk::Pixbuf> overlayPixbufs(const Glib::RefPtr<Gdk::Pixbuf>&,
                                                  const Glib::RefPtr<Gdk::Pixbuf>&);
  Glib::RefPtr<Gdk::Pixbuf> getIconByName(const std::string& name, int size);
  double getScaledIconSize();
  void makeMenu();
  bool hasMenuLayout() const;
  void queueMenuRebuild();
  void rebuildMenu();
  void appendMenuItems(GMenu* menu, DbusmenuMenuitem* parent, GSimpleActionGroup* actions);
  static void syncMenu(GMenu* shown, GMenuModel* model);
  static void onMenuNewItem(DbusmenuClient* client, DbusmenuMenuitem* item, Item* self);
  static void onMenuPropertyChanged(DbusmenuMenuitem* item, const gchar* name, GVariant* value,
                                    Item* self);
  static void onMenuActivate(GSimpleAction* action, GVariant* parameter, Item* self);
  static void onSubmenuShown(GSimpleAction* action, GVariant* value, Item* self);
  void handleClick(int n_press, double x, double y);
  bool handleScroll(double dx, double dy);

  // smooth scrolling threshold
  gdouble scroll_threshold_ = 0;
  gdouble distance_scrolled_x_ = 0;
  gdouble distance_scrolled_y_ = 0;
  // visibility of items with Status == Passive
  bool show_passive_ = false;
  bool show_tooltip_ = true;
  // hidden via config
  bool is_hidden_ = false;
  bool ready_ = false;
  // The popover shows a model built from the dbusmenu tree, rebuilt only when it is shown
  bool menu_dirty_ = true;
  sigc::connection menu_rebuild_;
  Glib::RefPtr<Gio::SimpleActionGroup> menu_actions_;
  Glib::RefPtr<Gtk::GestureClick> click_;
  Glib::RefPtr<Gtk::EventControllerScroll> scroll_;
  // Not Glib::ustring: its comparisons collate, which costs time linear in the item's string.
  std::string status_ = "active";

  const Bar& bar_;
  const std::function<void(Item&)> on_ready_;
  const std::function<void(Item&)> on_invalidate_;
  const std::function<void()> on_updated_;

  Glib::RefPtr<Gio::DBus::Proxy> proxy_;
  Glib::RefPtr<Gio::Cancellable> cancellable_;
  std::set<std::string_view> update_pending_;

  Host& host_;
  const ItemOrderMap& orders_;
};

}  // namespace waybar::modules::SNI
