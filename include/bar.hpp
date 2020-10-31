#pragma once

#include <gdkmm/monitor.h>
#include <glibmm/refptr.h>
#include <gtkmm/box.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <json/json.h>
#include "AModule.hpp"
#include "idle-inhibit-unstable-v1-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

namespace waybar {

class Factory;
struct waybar_output {
  Glib::RefPtr<Gdk::Monitor> monitor;
  std::string                name;

  std::unique_ptr<struct zxdg_output_v1, decltype(&zxdg_output_v1_destroy)> xdg_output = {
      nullptr, &zxdg_output_v1_destroy};
};

class Bar {
 public:
  Bar(struct waybar_output *w_output, const Json::Value &);
  Bar(const Bar &) = delete;
  ~Bar() = default;

  auto toggle() -> void;
  void handleSignal(int);

  struct waybar_output *output;
  Json::Value           config;
  struct wl_surface *   surface;
  bool                  visible = true;
  bool                  vertical = false;
  Gtk::Window           window;

 private:
  static constexpr const char *MIN_HEIGHT_MSG =
      "Requested height: {} exceeds the minimum height: {} required by the modules";
  static constexpr const char *MIN_WIDTH_MSG =
      "Requested width: {} exceeds the minimum width: {} required by the modules";
  static constexpr const char *BAR_SIZE_MSG =
      "Bar configured (width: {}, height: {}) for output: {}";
  static constexpr const char *SIZE_DEFINED =
      "{} size is defined in the config file so it will stay like that";
  static void layerSurfaceHandleConfigure(void *, struct zwlr_layer_surface_v1 *, uint32_t,
                                          uint32_t, uint32_t);
  static void layerSurfaceHandleClosed(void *, struct zwlr_layer_surface_v1 *);

#ifdef HAVE_GTK_LAYER_SHELL
  /* gtk-layer-shell code */
  void initGtkLayerShell();
  void onConfigureGLS(GdkEventConfigure *ev);
  void onMapGLS(GdkEventAny *ev);
#endif
  /* fallback layer-surface code */
  void onConfigure(GdkEventConfigure *ev);
  void onRealize();
  void onMap(GdkEventAny *ev);
  void setSurfaceSize(uint32_t width, uint32_t height);
  /* common code */
  void setExclusiveZone(uint32_t width, uint32_t height);
  auto setupWidgets() -> void;
  void getModules(const Factory &, const std::string &);
  void setupAltFormatKeyForModule(const std::string &module_name);
  void setupAltFormatKeyForModuleList(const char *module_list_name);

  struct margins {
    int top = 0;
    int right = 0;
    int bottom = 0;
    int left = 0;
  } margins_;
  struct zwlr_layer_surface_v1 *layer_surface_;
  // use gtk-layer-shell instead of handling layer surfaces directly
  bool                                          use_gls_ = false;
  uint32_t                                      width_ = 0;
  uint32_t                                      height_ = 1;
  uint8_t                                       anchor_;
  Gtk::Box                                      left_;
  Gtk::Box                                      center_;
  Gtk::Box                                      right_;
  Gtk::Box                                      box_;
  std::vector<std::unique_ptr<waybar::AModule>> modules_left_;
  std::vector<std::unique_ptr<waybar::AModule>> modules_center_;
  std::vector<std::unique_ptr<waybar::AModule>> modules_right_;
};

}  // namespace waybar
