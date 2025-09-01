#pragma once

#include <gdk/gdk.h>
#include <glibmm/refptr.h>
#include <gtkmm/box.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include <memory>
#include <string>

#include "AModule.hpp"
#include "bar.hpp"
#include "client.hpp"
#include "util/json.hpp"

namespace waybar::modules {

class KeyboardState : public AModule {
 public:
  KeyboardState(const std::string &, const waybar::Bar &, const Json::Value &);
  ~KeyboardState();
  void update();

 private:
  Gtk::Box box_;
  Gtk::Label numlock_label_;
  Gtk::Label capslock_label_;
  Gtk::Label scrolllock_label_;

  std::string numlock_format_;
  std::string capslock_format_;
  std::string scrolllock_format_;
  std::string icon_locked_;
  std::string icon_unlocked_;

  struct wl_seat *seat_;
  struct wl_keyboard *keyboard_;
  struct xkb_context *xkb_context_;
  struct xkb_state *xkb_state_;
  struct xkb_keymap *xkb_keymap_;

  void update_led(Gtk::Label *, std::string format, std::string name, bool locked);

 public:
  void register_seat(struct wl_registry *, uint32_t name, uint32_t version);
  void handle_keymap(uint32_t format, int32_t fd, uint32_t size);
  void handle_modifiers(uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked,
                        uint32_t group);
  void handle_seat_capabilities(unsigned int caps);
};

}  // namespace waybar::modules
