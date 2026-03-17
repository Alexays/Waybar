#include "modules/systemd_failed_units.hpp"

#include <fmt/format.h>
#include <giomm/dbusproxy.h>
#include <glibmm/variant.h>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <stdexcept>

static const unsigned UPDATE_DEBOUNCE_TIME_MS = 1000;

namespace waybar::modules {

SystemdFailedUnits::SystemdFailedUnits(const std::string& id, const Json::Value& config)
    : ALabel(config, "systemd-failed-units", id, "{nr_failed} failed", 1),
      hide_on_ok_(true),
      update_pending_(false),
      nr_failed_system_(0),
      nr_failed_user_(0),
      nr_failed_(0),
      last_status_() {
  if (config["hide-on-ok"].isBool()) {
    hide_on_ok_ = config["hide-on-ok"].asBool();
  }
  if (config["format-ok"].isString()) {
    format_ok_ = config["format-ok"].asString();
  } else {
    format_ok_ = format_;
  }

  /* Default to enable both "system" and "user". */
  if (!config["system"].isBool() || config["system"].asBool()) {
    system_props_proxy_ = Gio::DBus::Proxy::create_for_bus_sync(
        Gio::DBus::BusType::BUS_TYPE_SYSTEM, "org.freedesktop.systemd1",
        "/org/freedesktop/systemd1", "org.freedesktop.DBus.Properties");
    if (!system_props_proxy_) {
      throw std::runtime_error("Unable to connect to systemwide systemd DBus!");
    }
    system_props_proxy_->signal_signal().connect(
        sigc::mem_fun(*this, &SystemdFailedUnits::notify_cb));
  }
  if (!config["user"].isBool() || config["user"].asBool()) {
    user_props_proxy_ = Gio::DBus::Proxy::create_for_bus_sync(
        Gio::DBus::BusType::BUS_TYPE_SESSION, "org.freedesktop.systemd1",
        "/org/freedesktop/systemd1", "org.freedesktop.DBus.Properties");
    if (!user_props_proxy_) {
      throw std::runtime_error("Unable to connect to user systemd DBus!");
    }
    user_props_proxy_->signal_signal().connect(
        sigc::mem_fun(*this, &SystemdFailedUnits::notify_cb));
  }

  updateData();
  /* Always update for the first time. */
  dp.emit();
}

auto SystemdFailedUnits::notify_cb(const Glib::ustring& sender_name,
                                   const Glib::ustring& signal_name,
                                   const Glib::VariantContainerBase& arguments) -> void {
  if (signal_name == "PropertiesChanged" && !update_pending_) {
    update_pending_ = true;
    /* The fail count may fluctuate due to restarting. */
    Glib::signal_timeout().connect_once(sigc::mem_fun(*this, &SystemdFailedUnits::updateData),
                                        UPDATE_DEBOUNCE_TIME_MS);
  }
}

void SystemdFailedUnits::RequestSystemState() {
  auto load = [](const char* kind, Glib::RefPtr<Gio::DBus::Proxy>& proxy) -> std::string {
    try {
      if (!proxy) return "unknown";
      auto parameters = Glib::VariantContainerBase(
          g_variant_new("(ss)", "org.freedesktop.systemd1.Manager", "SystemState"));
      Glib::VariantContainerBase data = proxy->call_sync("Get", parameters);
      if (data && data.is_of_type(Glib::VariantType("(v)"))) {
        Glib::VariantBase variant;
        g_variant_get(const_cast<GVariant*>(data.gobj()), "(v)", &variant);
        if (variant && variant.is_of_type(Glib::VARIANT_TYPE_STRING)) {
          return g_variant_get_string(const_cast<GVariant*>(variant.gobj()), NULL);
        }
      }
    } catch (Glib::Error& e) {
      spdlog::error("Failed to get {} state: {}", kind, e.what().c_str());
    }
    return "unknown";
  };

  system_state_ = load("systemwide", system_props_proxy_);
  user_state_ = load("user", user_props_proxy_);
  if (system_state_ == "running" && user_state_ == "running")
    overall_state_ = "ok";
  else
    overall_state_ = "degraded";
}

void SystemdFailedUnits::RequestFailedUnits() {
  auto load = [](const char* kind, Glib::RefPtr<Gio::DBus::Proxy>& proxy) -> uint32_t {
    try {
      if (!proxy) return 0;
      auto parameters = Glib::VariantContainerBase(
          g_variant_new("(ss)", "org.freedesktop.systemd1.Manager", "NFailedUnits"));
      Glib::VariantContainerBase data = proxy->call_sync("Get", parameters);
      if (data && data.is_of_type(Glib::VariantType("(v)"))) {
        Glib::VariantBase variant;
        g_variant_get(const_cast<GVariant*>(data.gobj()), "(v)", &variant);
        if (variant && variant.is_of_type(Glib::VARIANT_TYPE_UINT32)) {
          return g_variant_get_uint32(const_cast<GVariant*>(variant.gobj()));
        }
      }
    } catch (Glib::Error& e) {
      spdlog::error("Failed to get {} failed units: {}", kind, e.what().c_str());
    }
    return 0;
  };

  nr_failed_system_ = load("systemwide", system_props_proxy_);
  nr_failed_user_ = load("user", user_props_proxy_);
  nr_failed_ = nr_failed_system_ + nr_failed_user_;
}

void SystemdFailedUnits::updateData() {
  update_pending_ = false;

  RequestSystemState();
  if (overall_state_ == "degraded") {
    RequestFailedUnits();
  } else {
    nr_failed_system_ = 0;
    nr_failed_user_ = 0;
    nr_failed_ = 0;
  }

  dp.emit();
}

auto SystemdFailedUnits::update() -> void {
  if (last_status_ == overall_state_) return;

  // Hide if needed.
  if (overall_state_ == "ok" && hide_on_ok_) {
    event_box_.set_visible(false);
    return;
  }

  event_box_.set_visible(true);

  // Set state class.
  if (!last_status_.empty() && label_.get_style_context()->has_class(last_status_)) {
    label_.get_style_context()->remove_class(last_status_);
  }
  if (!label_.get_style_context()->has_class(overall_state_)) {
    label_.get_style_context()->add_class(overall_state_);
  }

  last_status_ = overall_state_;

  label_.set_markup(fmt::format(
      fmt::runtime(nr_failed_ == 0 ? format_ok_ : format_), fmt::arg("nr_failed", nr_failed_),
      fmt::arg("nr_failed_system", nr_failed_system_),
      fmt::arg("nr_failed_user", nr_failed_user_), fmt::arg("system_state", system_state_),
      fmt::arg("user_state", user_state_), fmt::arg("overall_state", overall_state_)));
  ALabel::update();
}

}  // namespace waybar::modules
