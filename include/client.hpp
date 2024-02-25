#pragma once

#include <fmt/format.h>
#include <gdk/gdk.h>
#include <gdk/gdkwayland.h>
#include <wayland-client.h>

#include "bar.hpp"
#include "config.hpp"
#include "util/css_reload_helper.hpp"
#include "util/portal.hpp"

struct zwp_idle_inhibitor_v1;
struct zwp_idle_inhibit_manager_v1;

namespace waybar {

class Client {
 public:
  static Client *inst();
  int main(int argc, char *argv[]);
  void reset();

  Glib::RefPtr<Gtk::Application> gtk_app;
  Glib::RefPtr<Gdk::Display> gdk_display;
  struct wl_display *wl_display = nullptr;
  struct wl_registry *registry = nullptr;
  struct zxdg_output_manager_v1 *xdg_output_manager = nullptr;
  struct zwp_idle_inhibit_manager_v1 *idle_inhibit_manager = nullptr;
  std::vector<std::unique_ptr<Bar>> bars;
  Config config;
  std::string bar_id;

 private:
  Client() = default;
  const std::string getStyle(const std::string &style, std::optional<Appearance> appearance);
  void bindInterfaces();
  void handleOutput(struct waybar_output &output);
  auto setupCss(const std::string &css_file) -> void;
  struct waybar_output &getOutput(void *);
  std::vector<Json::Value> getOutputConfigs(struct waybar_output &output);

  static void handleGlobal(void *data, struct wl_registry *registry, uint32_t name,
                           const char *interface, uint32_t version);
  static void handleGlobalRemove(void *data, struct wl_registry *registry, uint32_t name);
  static void handleOutputDone(void *, struct zxdg_output_v1 *);
  static void handleOutputName(void *, struct zxdg_output_v1 *, const char *);
  static void handleOutputDescription(void *, struct zxdg_output_v1 *, const char *);
  void handleMonitorAdded(Glib::RefPtr<Gdk::Monitor> monitor);
  void handleMonitorRemoved(Glib::RefPtr<Gdk::Monitor> monitor);
  void handleDeferredMonitorRemoval(Glib::RefPtr<Gdk::Monitor> monitor);

  Glib::RefPtr<Gtk::StyleContext> style_context_;
  Glib::RefPtr<Gtk::CssProvider> css_provider_;
  std::unique_ptr<Portal> portal;
  std::list<struct waybar_output> outputs_;
  std::unique_ptr<CssReloadHelper> m_cssReloadHelper;
  std::string m_cssFile;
};

}  // namespace waybar
