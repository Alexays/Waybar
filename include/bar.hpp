#pragma once

#include <gdkmm/monitor.h>
#include <glibmm/refptr.h>
#include <gtkmm/box.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <json/json.h>

#include "AModule.hpp"
#include "xdg-output-unstable-v1-client-protocol.h"

namespace waybar {

class Factory;
struct waybar_output {
  Glib::RefPtr<Gdk::Monitor> monitor;
  std::string                name;
  std::string                identifier;

  std::unique_ptr<struct zxdg_output_v1, decltype(&zxdg_output_v1_destroy)> xdg_output = {
      nullptr, &zxdg_output_v1_destroy};
};

enum class bar_layer : uint8_t {
  BOTTOM,
  TOP,
  OVERLAY,
};

struct bar_margins {
  int top = 0;
  int right = 0;
  int bottom = 0;
  int left = 0;
};

class BarSurface {
 protected:
  BarSurface() = default;

 public:
  virtual void setExclusiveZone(bool enable) = 0;
  virtual void setLayer(bar_layer layer) = 0;
  virtual void setMargins(const struct bar_margins &margins) = 0;
  virtual void setPassThrough(bool enable) = 0;
  virtual void setPosition(const std::string_view &position) = 0;
  virtual void setSize(uint32_t width, uint32_t height) = 0;
  virtual void commit(){};

  virtual ~BarSurface() = default;
};

class Bar {
 public:
  Bar(struct waybar_output *w_output, const Json::Value &);
  Bar(const Bar &) = delete;
  ~Bar() = default;

  void setMode(const std::string &);
  void setVisible(bool visible);
  void toggle();
  void handleSignal(int);

  struct waybar_output *output;
  Json::Value           config;
  struct wl_surface *   surface;
  bool                  exclusive = true;
  bool                  visible = true;
  bool                  vertical = false;
  Gtk::Window           window;

 private:
  void onMap(GdkEventAny *);
  auto setupWidgets() -> void;
  void getModules(const Factory &, const std::string &);
  void setupAltFormatKeyForModule(const std::string &module_name);
  void setupAltFormatKeyForModuleList(const char *module_list_name);

  std::unique_ptr<BarSurface>                   surface_impl_;
  bar_layer                                     layer_;
  Gtk::Box                                      left_;
  Gtk::Box                                      center_;
  Gtk::Box                                      right_;
  Gtk::Box                                      box_;
  std::vector<std::unique_ptr<waybar::AModule>> modules_left_;
  std::vector<std::unique_ptr<waybar::AModule>> modules_center_;
  std::vector<std::unique_ptr<waybar::AModule>> modules_right_;
};

}  // namespace waybar
