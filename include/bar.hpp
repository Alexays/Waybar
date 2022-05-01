#pragma once

#include <gdkmm/monitor.h>
#include <glibmm/refptr.h>
#include <gtkmm/box.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <json/json.h>

#include <memory>
#include <vector>

#include "AModule.hpp"
#include "xdg-output-unstable-v1-client-protocol.h"

namespace waybar {

class Factory;
struct waybar_output {
  Glib::RefPtr<Gdk::Monitor> monitor;
  std::string name;
  std::string identifier;

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

struct bar_mode {
  bar_layer layer;
  bool exclusive;
  bool passthrough;
  bool visible;
};

#ifdef HAVE_SWAY
namespace modules::sway {
class BarIpcClient;
}
#endif  // HAVE_SWAY

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
  using bar_mode_map = std::map<std::string_view, struct bar_mode>;
  static const bar_mode_map PRESET_MODES;
  static const std::string_view MODE_DEFAULT;
  static const std::string_view MODE_INVISIBLE;

  Bar(struct waybar_output *w_output, const Json::Value &);
  Bar(const Bar &) = delete;
  ~Bar();

  void setMode(const std::string_view &);
  void setVisible(bool visible);
  void toggle();
  void handleSignal(int);

  struct waybar_output *output;
  Json::Value config;
  struct wl_surface *surface;
  bool visible = true;
  bool vertical = false;
  Gtk::Window window;

#ifdef HAVE_SWAY
  std::string bar_id;
#endif

 private:
  void onMap(GdkEventAny *);
  auto setupWidgets() -> void;
  void getModules(const Factory &, const std::string &, Gtk::Box *);
  void setupAltFormatKeyForModule(const std::string &module_name);
  void setupAltFormatKeyForModuleList(const char *module_list_name);
  void setMode(const bar_mode &);

  /* Copy initial set of modes to allow customization */
  bar_mode_map configured_modes = PRESET_MODES;
  std::string last_mode_{MODE_DEFAULT};

  std::unique_ptr<BarSurface> surface_impl_;
  Gtk::Box left_;
  Gtk::Box center_;
  Gtk::Box right_;
  Gtk::Box box_;
  std::vector<std::shared_ptr<waybar::AModule>> modules_left_;
  std::vector<std::shared_ptr<waybar::AModule>> modules_center_;
  std::vector<std::shared_ptr<waybar::AModule>> modules_right_;
#ifdef HAVE_SWAY
  using BarIpcClient = modules::sway::BarIpcClient;
  std::unique_ptr<BarIpcClient> _ipc_client;
#endif
  std::vector<std::shared_ptr<waybar::AModule>> modules_all_;
};

}  // namespace waybar
