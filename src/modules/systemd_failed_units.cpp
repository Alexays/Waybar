#include "modules/systemd_failed_units.hpp"

#include <giomm/dbusproxy.h>
#include <glibmm/variant.h>
#include <spdlog/spdlog.h>

#include <cstdint>

static const unsigned UPDATE_DEBOUNCE_TIME_MS = 1000;

namespace waybar::modules {

SystemdFailedUnits::SystemdFailedUnits(const std::string& id, const Json::Value& config)
    : ALabel(config, "systemd-failed-units", id, "{nr_failed} failed", 1),
      hide_on_ok(true),
      update_pending(false),
      nr_failed_system(0),
      nr_failed_user(0),
      nr_failed(0),
      last_status() {
  if (config["hide-on-ok"].isBool()) {
    hide_on_ok = config["hide-on-ok"].asBool();
  }
  if (config["format-ok"].isString()) {
    format_ok = config["format-ok"].asString();
  } else {
    format_ok = format_;
  }

  /* Default to enable both "system" and "user". */
  if (!config["system"].isBool() || config["system"].asBool()) {
    system_proxy = Gio::DBus::Proxy::create_for_bus_sync(
        Gio::DBus::BusType::BUS_TYPE_SYSTEM, "org.freedesktop.systemd1",
        "/org/freedesktop/systemd1", "org.freedesktop.DBus.Properties");
    if (!system_proxy) {
      throw std::runtime_error("Unable to connect to systemwide systemd DBus!");
    }
    system_proxy->signal_signal().connect(sigc::mem_fun(*this, &SystemdFailedUnits::notify_cb));
  }
  if (!config["user"].isBool() || config["user"].asBool()) {
    user_proxy = Gio::DBus::Proxy::create_for_bus_sync(
        Gio::DBus::BusType::BUS_TYPE_SESSION, "org.freedesktop.systemd1",
        "/org/freedesktop/systemd1", "org.freedesktop.DBus.Properties");
    if (!user_proxy) {
      throw std::runtime_error("Unable to connect to user systemd DBus!");
    }
    user_proxy->signal_signal().connect(sigc::mem_fun(*this, &SystemdFailedUnits::notify_cb));
  }

  updateData();
  /* Always update for the first time. */
  dp.emit();
}

SystemdFailedUnits::~SystemdFailedUnits() {
  if (system_proxy) system_proxy.reset();
  if (user_proxy) user_proxy.reset();
}

auto SystemdFailedUnits::notify_cb(const Glib::ustring& sender_name,
                                   const Glib::ustring& signal_name,
                                   const Glib::VariantContainerBase& arguments) -> void {
  if (signal_name == "PropertiesChanged" && !update_pending) {
    update_pending = true;
    /* The fail count may fluctuate due to restarting. */
    Glib::signal_timeout().connect_once(sigc::mem_fun(*this, &SystemdFailedUnits::updateData),
                                        UPDATE_DEBOUNCE_TIME_MS);
  }
}

void SystemdFailedUnits::updateData() {
  update_pending = false;

  auto load = [](const char* kind, Glib::RefPtr<Gio::DBus::Proxy>& proxy) -> uint32_t {
    try {
      auto parameters = Glib::VariantContainerBase(
          g_variant_new("(ss)", "org.freedesktop.systemd1.Manager", "NFailedUnits"));
      Glib::VariantContainerBase data = proxy->call_sync("Get", parameters);
      if (data && data.is_of_type(Glib::VariantType("(v)"))) {
        Glib::VariantBase variant;
        g_variant_get(data.gobj_copy(), "(v)", &variant);
        if (variant && variant.is_of_type(Glib::VARIANT_TYPE_UINT32)) {
          uint32_t value = 0;
          g_variant_get(variant.gobj_copy(), "u", &value);
          return value;
        }
      }
    } catch (Glib::Error& e) {
      spdlog::error("Failed to get {} failed units: {}", kind, e.what().c_str());
    }
    return 0;
  };

  if (system_proxy) {
    nr_failed_system = load("systemwide", system_proxy);
  }
  if (user_proxy) {
    nr_failed_user = load("user", user_proxy);
  }
  dp.emit();
}

auto SystemdFailedUnits::update() -> void {
  nr_failed = nr_failed_system + nr_failed_user;

  // Hide if needed.
  if (nr_failed == 0 && hide_on_ok) {
    event_box_.set_visible(false);
    return;
  }
  if (!event_box_.get_visible()) {
    event_box_.set_visible(true);
  }

  // Set state class.
  const std::string status = nr_failed == 0 ? "ok" : "degraded";
  if (!last_status.empty() && label_.get_style_context()->has_class(last_status)) {
    label_.get_style_context()->remove_class(last_status);
  }
  if (!label_.get_style_context()->has_class(status)) {
    label_.get_style_context()->add_class(status);
  }
  last_status = status;

  label_.set_markup(fmt::format(
      fmt::runtime(nr_failed == 0 ? format_ok : format_), fmt::arg("nr_failed", nr_failed),
      fmt::arg("nr_failed_system", nr_failed_system), fmt::arg("nr_failed_user", nr_failed_user)));
  ALabel::update();
}

}  // namespace waybar::modules
