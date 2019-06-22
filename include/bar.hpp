#pragma once

#include <glibmm/refptr.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/box.h>
#include <json/json.h>
#include "AModule.hpp"
#include "idle-inhibit-unstable-v1-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

namespace waybar {

class Factory;
struct waybar_output {
  struct wl_output *     output;
  std::string            name;
  uint32_t               wl_name;
  struct zxdg_output_v1 *xdg_output;
};

class Bar {
 public:
  Bar(struct waybar_output *w_output, const Json::Value &);
  Bar(const Bar &) = delete;
  ~Bar() = default;

  auto toggle() -> void;
  void handleSignal(int);

  struct waybar_output *        output;
  Json::Value                   config;
  Gtk::Window                   window;
  struct wl_surface *           surface;
  struct zwlr_layer_surface_v1 *layer_surface;
  bool                          visible = true;
  bool                          vertical = false;

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

  void destroyOutput();
  void onConfigure(GdkEventConfigure *ev);
  void onRealize();
  void onMap(GdkEventAny *ev);
  void setMarginsAndZone(uint32_t height, uint32_t width);
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
