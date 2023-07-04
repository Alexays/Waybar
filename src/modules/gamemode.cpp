#include "modules/gamemode.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "AModule.hpp"
#include "giomm/dbusconnection.h"
#include "giomm/dbusinterface.h"
#include "giomm/dbusproxy.h"
#include "giomm/dbuswatchname.h"
#include "glibmm/error.h"
#include "glibmm/ustring.h"
#include "glibmm/variant.h"
#include "glibmm/varianttype.h"
#include "gtkmm/label.h"
#include "gtkmm/tooltip.h"
#include "util/gtk_icon.hpp"

namespace waybar::modules {
Gamemode::Gamemode(const std::string& id, const Json::Value& config)
    : AModule(config, "gamemode", id), box_(Gtk::ORIENTATION_HORIZONTAL, 0), icon_(), label_() {
  box_.pack_start(icon_);
  box_.pack_start(label_);
  box_.set_name(name_);
  event_box_.add(box_);

  // Tooltip
  if (config_["tooltip"].isBool()) {
    tooltip = config_["tooltip"].asBool();
  }
  box_.set_has_tooltip(tooltip);

  // Tooltip Format
  if (config_["tooltip-format"].isString()) {
    tooltip_format = config_["tooltip-format"].asString();
  }

  // Hide when game count is 0
  if (config_["hide-not-running"].isBool()) {
    hideNotRunning = config_["hide-not-running"].asBool();
  }

  // Icon Name
  if (config_["icon-name"].isString()) {
    iconName = config_["icon-name"].asString();
  }

  // Icon Spacing
  if (config_["icon-spacing"].isUInt()) {
    iconSpacing = config_["icon-spacing"].asUInt();
  }
  box_.set_spacing(iconSpacing);

  // Whether to use icon or not
  if (config_["use-icon"].isBool()) {
    useIcon = config_["use-icon"].asBool();
  }

  // Icon Size
  if (config_["icon-size"].isUInt()) {
    iconSize = config_["icon-size"].asUInt();
  }
  icon_.set_pixel_size(iconSize);

  // Format
  if (config_["format"].isString()) {
    format = config_["format"].asString();
  }

  // Format Alt
  if (config_["format-alt"].isString()) {
    format_alt = config_["format-alt"].asString();
  }

  // Glyph
  if (config_["glyph"].isString()) {
    glyph = config_["glyph"].asString();
  }

  gamemodeWatcher_id = Gio::DBus::watch_name(
      Gio::DBus::BUS_TYPE_SESSION, dbus_name, sigc::mem_fun(*this, &Gamemode::appear),
      sigc::mem_fun(*this, &Gamemode::disappear),
      Gio::DBus::BusNameWatcherFlags::BUS_NAME_WATCHER_FLAGS_AUTO_START);

  // Connect to gamemode
  gamemode_proxy = Gio::DBus::Proxy::create_for_bus_sync(Gio::DBus::BusType::BUS_TYPE_SESSION,
                                                         dbus_name, dbus_obj_path, dbus_interface);
  if (!gamemode_proxy) {
    throw std::runtime_error("Unable to connect to gamemode DBus!...");
  } else {
    gamemode_proxy->signal_signal().connect(sigc::mem_fun(*this, &Gamemode::notify_cb));
  }

  // Connect to Login1 PrepareForSleep signal
  system_connection = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::BUS_TYPE_SYSTEM);
  if (!system_connection) {
    throw std::runtime_error("Unable to connect to the SYSTEM Bus!...");
  } else {
    login1_id = system_connection->signal_subscribe(
        sigc::mem_fun(*this, &Gamemode::prepareForSleep_cb), "org.freedesktop.login1",
        "org.freedesktop.login1.Manager", "PrepareForSleep", "/org/freedesktop/login1");
  }

  event_box_.signal_button_press_event().connect(sigc::mem_fun(*this, &Gamemode::handleToggle));
}

Gamemode::~Gamemode() {
  if (gamemode_proxy) gamemode_proxy.reset();
  if (gamemodeWatcher_id > 0) {
    Gio::DBus::unwatch_name(gamemodeWatcher_id);
    gamemodeWatcher_id = 0;
  }
  if (login1_id > 0) {
    system_connection->signal_unsubscribe(login1_id);
    login1_id = 0;
  }
}

// Gets the DBus ClientCount
void Gamemode::getData() {
  if (gamemodeRunning && gamemode_proxy) {
    try {
      // Get game count
      auto parameters = Glib::VariantContainerBase(
          g_variant_new("(ss)", dbus_get_interface.c_str(), "ClientCount"));
      Glib::VariantContainerBase data = gamemode_proxy->call_sync("Get", parameters);
      if (data && data.is_of_type(Glib::VariantType("(v)"))) {
        Glib::VariantBase variant;
        g_variant_get(data.gobj_copy(), "(v)", &variant);
        if (variant && variant.is_of_type(Glib::VARIANT_TYPE_INT32)) {
          g_variant_get(variant.gobj_copy(), "i", &gameCount);
          return;
        }
      }
    } catch (Glib::Error& e) {
      spdlog::error("Gamemode Error {}", e.what().c_str());
    }
  }
  gameCount = 0;
}

// Whenever the DBus ClientCount changes
void Gamemode::notify_cb(const Glib::ustring& sender_name, const Glib::ustring& signal_name,
                         const Glib::VariantContainerBase& arguments) {
  if (signal_name == "PropertiesChanged") {
    getData();
    dp.emit();
  }
}

void Gamemode::prepareForSleep_cb(const Glib::RefPtr<Gio::DBus::Connection>& connection,
                                  const Glib::ustring& sender_name,
                                  const Glib::ustring& object_path,
                                  const Glib::ustring& interface_name,
                                  const Glib::ustring& signal_name,
                                  const Glib::VariantContainerBase& parameters) {
  if (parameters.is_of_type(Glib::VariantType("(b)"))) {
    gboolean sleeping;
    g_variant_get(parameters.gobj_copy(), "(b)", &sleeping);
    if (!sleeping) {
      getData();
      dp.emit();
    }
  }
}

// When the gamemode name appears
void Gamemode::appear(const Glib::RefPtr<Gio::DBus::Connection>& connection,
                      const Glib::ustring& name, const Glib::ustring& name_owner) {
  gamemodeRunning = true;
  event_box_.set_visible(true);
  getData();
  dp.emit();
}
// When the gamemode name disappears
void Gamemode::disappear(const Glib::RefPtr<Gio::DBus::Connection>& connection,
                         const Glib::ustring& name) {
  gamemodeRunning = false;
  event_box_.set_visible(false);
}

bool Gamemode::handleToggle(GdkEventButton* const& event) {
  showAltText = !showAltText;
  dp.emit();
  return true;
}

auto Gamemode::update() -> void {
  // Don't update widget if the Gamemode service isn't running
  if (!gamemodeRunning || (gameCount <= 0 && hideNotRunning)) {
    event_box_.set_visible(false);
    return;
  }

  // Show the module
  if (!event_box_.get_visible()) event_box_.set_visible(true);

  // CSS status class
  const std::string status = gamemodeRunning && gameCount > 0 ? "running" : "";
  // Remove last status if it exists
  if (!lastStatus.empty() && box_.get_style_context()->has_class(lastStatus)) {
    box_.get_style_context()->remove_class(lastStatus);
  }
  // Add the new status class to the Box
  if (!status.empty() && !box_.get_style_context()->has_class(status)) {
    box_.get_style_context()->add_class(status);
  }
  lastStatus = status;

  // Tooltip
  if (tooltip) {
    std::string text = fmt::format(fmt::runtime(tooltip_format), fmt::arg("count", gameCount));
    box_.set_tooltip_text(text);
  }

  // Label format
  std::string str = fmt::format(fmt::runtime(showAltText ? format_alt : format),
                                fmt::arg("glyph", useIcon ? "" : glyph),
                                fmt::arg("count", gameCount > 0 ? std::to_string(gameCount) : ""));
  label_.set_markup(str);

  if (useIcon) {
    if (!DefaultGtkIconThemeWrapper::has_icon(iconName)) {
      iconName = DEFAULT_ICON_NAME;
    }
    icon_.set_from_icon_name(iconName, Gtk::ICON_SIZE_INVALID);
  }

  // Call parent update
  AModule::update();
}

}  // namespace waybar::modules
