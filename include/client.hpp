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
  Client();
  void              setupConfigs(const std::string &config, const std::string &style);
  void              bindInterfaces();
  const std::string getValidPath(std::vector<std::string> paths);
  void              handleOutput(std::unique_ptr<struct waybar_output> &output);
  bool isValidOutput(const Json::Value &config, std::unique_ptr<struct waybar_output> &output);
  auto setupConfig() -> void;
  auto setupCss() -> void;

  static void handleGlobal(void *data, struct wl_registry *registry, uint32_t name,
                           const char *interface, uint32_t version);
  static void handleGlobalRemove(void *data, struct wl_registry *registry, uint32_t name);
  static void handleLogicalPosition(void *, struct zxdg_output_v1 *, int32_t, int32_t);
  static void handleLogicalSize(void *, struct zxdg_output_v1 *, int32_t, int32_t);
  static void handleDone(void *, struct zxdg_output_v1 *);
  static void handleName(void *, struct zxdg_output_v1 *, const char *);
  static void handleDescription(void *, struct zxdg_output_v1 *, const char *);

  Json::Value                                        config_;
  std::string                                        css_file_;
  std::string                                        config_file_;
  Glib::RefPtr<Gtk::StyleContext>                    style_context_;
  Glib::RefPtr<Gtk::CssProvider>                     css_provider_;
  std::vector<std::unique_ptr<struct waybar_output>> outputs_;
};

}  // namespace waybar
