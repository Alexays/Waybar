#pragma once

#include <giomm/dbusconnection.h>

#include <atomic>

#include "giomm/dbusproxy.h"

namespace waybar::util::GeoClueBackend {

class GeoClueBackend {
 private:
  guint watcherID_;
  sigc::connection signal_conn;
  Glib::RefPtr<Gio::DBus::Proxy> proxy;
  bool connected;

  /* Hack to keep constructor inaccessible but still public.
   * This is required to be able to use std::make_shared.
   * It is important to keep this class only accessible via a reference-counted
   * pointer because the destructor will manually free memory, and this could be
   * a problem with C++20's copy and move semantics.
   */
  struct PrivateConstructorTag {};

 public:
  sigc::signal<void> in_use_changed_signal_event;

  std::atomic<bool> location_in_use;  // GeoClue is being used

  static std::shared_ptr<GeoClueBackend> getInstance();

  // DBus callbacks
  void onAppear(const Glib::RefPtr<Gio::DBus::Connection>&, const Glib::ustring&,
                const Glib::ustring&);
  void onVanished(const Glib::RefPtr<Gio::DBus::Connection>&, const Glib::ustring&);
  void propertyChanged(const Gio::DBus::Proxy::MapChangedProperties& changedProperties,
                       const std::vector<Glib::ustring>& invalidatedProperties);

  GeoClueBackend(PrivateConstructorTag tag);
  ~GeoClueBackend();
};
}  // namespace waybar::util::GeoClueBackend
