#pragma once

#include <gtkmm.h>
#include <dbus-status-notifier-watcher.h>

namespace waybar::modules::SNI {

typedef enum {
  GF_WATCH_TYPE_HOST,
  GF_WATCH_TYPE_ITEM
} GfWatchType;

typedef struct {
  GfWatchType type;
  gchar* service;
  gchar* bus_name;
  gchar* object_path;
  guint watch_id;
} GfWatch;

class Watcher {
  public:
    Watcher();
    ~Watcher();
  private:
    static void busAcquired(GDBusConnection*, const gchar*, gpointer);
    static gboolean handleRegisterHost(Watcher*,
      GDBusMethodInvocation*, const gchar*);
    static gboolean handleRegisterItem(Watcher*,
      GDBusMethodInvocation*, const gchar*);
    static GfWatch* gfWatchFind(GSList* list, const gchar* bus_name,
      const gchar* object_path);
    static GfWatch* gfWatchNew(GfWatchType type,
      const gchar* service, const gchar* bus_name, const gchar* object_path);
    static void nameVanished(GDBusConnection* connection, const char* name,
      gpointer data);

    void updateRegisteredItems(SnOrgKdeStatusNotifierWatcher* obj);

    uint32_t bus_name_id_;
    uint32_t watcher_id_;
    GSList* hosts_ = nullptr;
    GSList* items_ = nullptr;
    SnOrgKdeStatusNotifierWatcher *watcher_ = nullptr;
};

}
