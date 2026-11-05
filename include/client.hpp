#pragma once

#include <fmt/format.h>
#include <gdk/wayland/gdkwayland.h>
#include <glibmm/main.h>
#include <gtkmm/cssprovider.h>
#include <wayland-client.h>

#include "bar.hpp"
#include "config.hpp"
#include "util/css_reload_helper.hpp"
#include "util/portal.hpp"

struct zwp_idle_inhibitor_v1;
struct zwp_idle_inhibit_manager_v1;
struct ext_idle_notifier_v1;
#ifdef HAVE_BAR_BLUR
struct ext_background_effect_manager_v1;
#endif

namespace waybar {

class Client {
 public:
  static Client* inst();
  int main(int argc, char* argv[]);
  void reset();

  Glib::RefPtr<Glib::MainLoop> main_loop;
  Glib::RefPtr<Gdk::Display> gdk_display;
  struct wl_display* wl_display = nullptr;
  struct wl_registry* registry = nullptr;
  struct zxdg_output_manager_v1* xdg_output_manager = nullptr;
  struct zwp_idle_inhibit_manager_v1* idle_inhibit_manager = nullptr;
  struct ext_idle_notifier_v1* idle_notifier = nullptr;
#ifdef HAVE_BAR_BLUR
  struct ext_background_effect_manager_v1* background_effect_manager = nullptr;
#endif
  std::vector<std::unique_ptr<Bar>> bars;
  Config config;
  std::string bar_id;

 private:
  Client() = default;
  const std::string getStyle(const std::string& style, std::optional<Appearance> appearance);
  void bindInterfaces();
  void handleOutput(struct waybar_output& output);
  auto setupCss(const std::string& css_file) -> void;
  struct waybar_output& getOutput(void*);
  std::vector<Json::Value> getOutputConfigs(struct waybar_output& output);

  static void handleGlobal(void* data, struct wl_registry* registry, uint32_t name,
                           const char* interface, uint32_t version);
  static void handleGlobalRemove(void* data, struct wl_registry* registry, uint32_t name);
  static void handleOutputLogicalSize(void*, struct zxdg_output_v1*, int32_t, int32_t);
  static void handleOutputDone(void*, struct zxdg_output_v1*);
  void createBarsBatch();
  static void handleOutputName(void*, struct zxdg_output_v1*, const char*);
  static void handleOutputDescription(void*, struct zxdg_output_v1*, const char*);
  void handleMonitorsChanged(guint position, guint removed, guint added);
  void handleMonitorAdded(Glib::RefPtr<Gdk::Monitor> monitor);
  void handleDeferredMonitorRemoval(Glib::RefPtr<Gdk::Monitor> monitor);

  Glib::RefPtr<Gtk::CssProvider> css_provider_;
  Glib::RefPtr<Gtk::CssProvider> compat_css_provider_;
  Glib::RefPtr<Gtk::CssProvider> menu_css_provider_;
  std::unique_ptr<Portal> portal;
  std::list<struct waybar_output> outputs_;
  std::unique_ptr<CssReloadHelper> m_cssReloadHelper;
  std::string m_cssFile;
  sigc::connection monitors_changed_connection_;
  std::list<waybar_output*> pending_outputs_;
  bool bars_scheduled_ = false;
};

}  // namespace waybar
