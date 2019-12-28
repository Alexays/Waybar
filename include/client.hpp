#pragma once

#include <fmt/format.h>
#include <gdk/gdk.h>
#include <gdk/gdkwayland.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wordexp.h>
#include "bar.hpp"

namespace waybar {

class Client {
 public:
  static Client *inst();
  int            main(int argc, char *argv[]);

  Glib::RefPtr<Gtk::Application>      gtk_app;
  Glib::RefPtr<Gdk::Display>          gdk_display;
  struct wl_display *                 wl_display = nullptr;
  struct wl_registry *                registry = nullptr;
  struct zwlr_layer_shell_v1 *        layer_shell = nullptr;
  struct zxdg_output_manager_v1 *     xdg_output_manager = nullptr;
  struct zwp_idle_inhibit_manager_v1 *idle_inhibit_manager = nullptr;
  std::vector<std::unique_ptr<Bar>>   bars;

 private:
  Client() = default;
  std::tuple<const std::string, const std::string> getConfigs(const std::string &config,
                                                              const std::string &style) const;
  void                                             bindInterfaces();
  const std::string getValidPath(const std::vector<std::string> &paths) const;
  void              handleOutput(std::unique_ptr<struct waybar_output> &output);
  bool isValidOutput(const Json::Value &config, std::unique_ptr<struct waybar_output> &output);
  auto setupConfig(const std::string &config_file) -> void;
  auto setupCss(const std::string &css_file) -> void;
  std::unique_ptr<struct waybar_output> &getOutput(void *);
  std::vector<Json::Value> getOutputConfigs(std::unique_ptr<struct waybar_output> &output);

  static void handleGlobal(void *data, struct wl_registry *registry, uint32_t name,
                           const char *interface, uint32_t version);
  static void handleGlobalRemove(void *data, struct wl_registry *registry, uint32_t name);
  static void handleOutputName(void *, struct zxdg_output_v1 *, const char *);
  void        handleMonitorAdded(Glib::RefPtr<Gdk::Monitor> monitor);
  void        handleMonitorRemoved(Glib::RefPtr<Gdk::Monitor> monitor);

  Json::Value                                        config_;
  Glib::RefPtr<Gtk::StyleContext>                    style_context_;
  Glib::RefPtr<Gtk::CssProvider>                     css_provider_;
  std::vector<std::unique_ptr<struct waybar_output>> outputs_;
};

}  // namespace waybar
