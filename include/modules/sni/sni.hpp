#pragma once

#include <gtkmm.h>
#include <dbus-status-notifier-item.h>

namespace waybar::modules::SNI {

class Item {
  public:
    Item(std::string, std::string, Glib::Dispatcher*);

    std::string bus_name;
    std::string object_path;
    Gtk::EventBox event_box;

    int icon_size;
    int effective_icon_size;
    Gtk::Image* image;
    std::string category;
    std::string id;
    std::string status;

    std::string title;
    int32_t window_id;
    std::string icon_name;
    std::string overlay_icon_name;
    std::string attention_icon_name;
    std::string attention_movie_name;
    std::string icon_theme_path;
    std::string menu;
    bool item_is_menu;
  private:
    static void proxyReady(GObject* obj, GAsyncResult* res, gpointer data);
    static void getAll(GObject* obj, GAsyncResult* res, gpointer data);

    void updateImage();
    Glib::RefPtr<Gdk::Pixbuf> getIconByName(std::string name, int size);
    Glib::Dispatcher* dp_;
    GCancellable* cancellable_ = nullptr;
    SnOrgKdeStatusNotifierItem* proxy_ = nullptr;
};

}
