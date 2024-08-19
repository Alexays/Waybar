#pragma once

#include <gdkmm/monitor.h>
#include <glibmm/refptr.h>
#include <gtkmm/box.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <json/json.h>

#include <memory>
#include <optional>
#include <vector>

#include "AModule.hpp"
#include "group.hpp"
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
  std::optional<bar_layer> layer;
  bool exclusive;
  bool passthrough;
  bool visible;
};

#ifdef HAVE_SWAY
namespace modules::sway {
class BarIpcClient;
}
#endif  // HAVE_SWAY

class Bar : public sigc::trackable {
 public:
  using bar_mode_map = std::map<std::string, struct bar_mode>;
  static const bar_mode_map PRESET_MODES;
  static const std::string MODE_DEFAULT;
  static const std::string MODE_INVISIBLE;

  Bar(struct waybar_output *w_output, const Json::Value &);
  Bar(const Bar &) = delete;
  ~Bar();

  void setMode(const std::string &mode);
  void setVisible(bool value);
  void toggle();
  void handleSignal(int);

  struct waybar_output *output;
  Json::Value config;
  struct wl_surface *surface;
  bool visible = true;
  Gtk::Window window;
  Gtk::Orientation orientation = Gtk::ORIENTATION_HORIZONTAL;
  Gtk::PositionType position = Gtk::POS_TOP;

  int x_global;
  int y_global;

#ifdef HAVE_SWAY
  std::string bar_id;
#endif

 private:
  void onMap(GdkEventAny *);
  auto setupWidgets() -> void;
  void getModules(const Factory &, const std::string &, waybar::Group *);
  void setupAltFormatKeyForModule(const std::string &module_name);
  void setupAltFormatKeyForModuleList(const char *module_list_name);
  void setMode(const bar_mode &);
  void setPassThrough(bool passthrough);
  void setPosition(Gtk::PositionType position);
  void onConfigure(GdkEventConfigure *ev);
  void configureGlobalOffset(int width, int height);
  void onOutputGeometryChanged();

  /* Copy initial set of modes to allow customization */
  bar_mode_map configured_modes = PRESET_MODES;
  std::string last_mode_{MODE_DEFAULT};

  struct bar_margins margins_;
  uint32_t width_, height_;
  bool passthrough_;

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
