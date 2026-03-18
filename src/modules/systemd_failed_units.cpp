#include "modules/systemd_failed_units.hpp"

#include <fmt/format.h>
#include <giomm/dbusproxy.h>
#include <glibmm/markup.h>
#include <glibmm/variant.h>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <exception>
#include <stdexcept>
#include <tuple>

static const unsigned UPDATE_DEBOUNCE_TIME_MS = 1000;

namespace waybar::modules {

SystemdFailedUnits::SystemdFailedUnits(const std::string& id, const Json::Value& config,
                                       std::mutex& reap_mtx, std::list<pid_t>& reap)
    : ALabel(config, "systemd-failed-units", id, "{nr_failed} failed", reap_mtx, reap, 1),
      hide_on_ok_(true),
      tooltip_format_(
          "System: {system_state}\nUser: {user_state}\nFailed units ({nr_failed}):\n"
          "{failed_units_list}"),
      tooltip_format_ok_("System: {system_state}\nUser: {user_state}"),
      tooltip_unit_format_("{name}: {description}"),
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

  if (config["tooltip-format"].isString()) {
    tooltip_format_ = config["tooltip-format"].asString();
  }
  if (config["tooltip-format-ok"].isString()) {
    tooltip_format_ok_ = config["tooltip-format-ok"].asString();
  }
  if (config["tooltip-unit-format"].isString()) {
    tooltip_unit_format_ = config["tooltip-unit-format"].asString();
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
    try {
      system_manager_proxy_ = Gio::DBus::Proxy::create_for_bus_sync(
          Gio::DBus::BusType::BUS_TYPE_SYSTEM, "org.freedesktop.systemd1",
          "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager");
    } catch (const Glib::Error& e) {
      spdlog::warn("Unable to connect to systemwide systemd Manager interface: {}",
                   e.what().c_str());
    }
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
    try {
      user_manager_proxy_ = Gio::DBus::Proxy::create_for_bus_sync(
          Gio::DBus::BusType::BUS_TYPE_SESSION, "org.freedesktop.systemd1",
          "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager");
    } catch (const Glib::Error& e) {
      spdlog::warn("Unable to connect to user systemd Manager interface: {}", e.what().c_str());
    }
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

void SystemdFailedUnits::RequestFailedUnitsList() {
  failed_units_.clear();
  if (!tooltipEnabled() || nr_failed_ == 0) {
    return;
  }

  if (system_manager_proxy_) {
    auto units = LoadFailedUnitsList("systemwide", system_manager_proxy_, "system");
    failed_units_.insert(failed_units_.end(), units.begin(), units.end());
  }
  if (user_manager_proxy_) {
    auto units = LoadFailedUnitsList("user", user_manager_proxy_, "user");
    failed_units_.insert(failed_units_.end(), units.begin(), units.end());
  }
}

auto SystemdFailedUnits::LoadFailedUnitsList(const char* kind,
                                             Glib::RefPtr<Gio::DBus::Proxy>& proxy,
                                             const std::string& scope) -> std::vector<FailedUnit> {
  // org.freedesktop.systemd1.Manager.ListUnits returns
  // (name, description, load_state, active_state, sub_state, followed, unit_path, job_id,
  //  job_type, job_path).
  using UnitRow = std::tuple<Glib::ustring, Glib::ustring, Glib::ustring, Glib::ustring,
                             Glib::ustring, Glib::ustring, Glib::DBusObjectPathString, guint32,
                             Glib::ustring, Glib::DBusObjectPathString>;
  using ListUnitsReply = Glib::Variant<std::tuple<std::vector<UnitRow>>>;

  std::vector<FailedUnit> units;
  if (!proxy) {
    return units;
  }

  try {
    auto data = proxy->call_sync("ListUnits");
    if (!data) return units;
    if (!data.is_of_type(ListUnitsReply::variant_type())) {
      spdlog::warn("Unexpected DBus signature for ListUnits: {}", data.get_type_string());
      return units;
    }

    auto [rows] = Glib::VariantBase::cast_dynamic<ListUnitsReply>(data).get();
    for (const auto& row : rows) {
      const auto& name = std::get<0>(row);
      const auto& description = std::get<1>(row);
      const auto& load_state = std::get<2>(row);
      const auto& active_state = std::get<3>(row);
      const auto& sub_state = std::get<4>(row);
      if (active_state == "failed" || sub_state == "failed") {
        units.push_back({name, description, load_state, active_state, sub_state, scope});
      }
    }
  } catch (const Glib::Error& e) {
    spdlog::error("Failed to list {} units: {}", kind, e.what().c_str());
  }

  return units;
}

std::string SystemdFailedUnits::BuildTooltipFailedList() const {
  if (failed_units_.empty()) {
    return "";
  }

  std::string list;
  list.reserve(failed_units_.size() * 16);
  bool first = true;
  for (const auto& unit : failed_units_) {
    try {
      auto line = fmt::format(
          fmt::runtime(tooltip_unit_format_),
          fmt::arg("name", Glib::Markup::escape_text(unit.name).raw()),
          fmt::arg("description", Glib::Markup::escape_text(unit.description).raw()),
          fmt::arg("load_state", unit.load_state), fmt::arg("active_state", unit.active_state),
          fmt::arg("sub_state", unit.sub_state), fmt::arg("scope", unit.scope));
      if (!first) {
        list += "\n";
      }
      first = false;
      list += "- ";
      list += line;
    } catch (const std::exception& e) {
      spdlog::warn("Failed to format tooltip for unit {}: {}", unit.name, e.what());
    }
  }

  return list;
}

void SystemdFailedUnits::updateData() {
  update_pending_ = false;

  RequestSystemState();
  if (overall_state_ == "degraded") {
    RequestFailedUnits();
    RequestFailedUnitsList();
  } else {
    nr_failed_system_ = nr_failed_user_ = nr_failed_ = 0;
    failed_units_.clear();
  }

  dp.emit();
}

auto SystemdFailedUnits::update() -> void {
  // Hide if needed.
  if (overall_state_ == "ok" && hide_on_ok_) {
    event_box_.set_visible(false);
    last_status_ = overall_state_;
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
      fmt::arg("nr_failed_system", nr_failed_system_), fmt::arg("nr_failed_user", nr_failed_user_),
      fmt::arg("system_state", system_state_), fmt::arg("user_state", user_state_),
      fmt::arg("overall_state", overall_state_)));
  if (tooltipEnabled()) {
    std::string failed_list = BuildTooltipFailedList();
    auto tooltip_template = overall_state_ == "ok" ? tooltip_format_ok_ : tooltip_format_;
    if (!tooltip_template.empty()) {
      label_.set_tooltip_markup(fmt::format(
          fmt::runtime(tooltip_template), fmt::arg("nr_failed", nr_failed_),
          fmt::arg("nr_failed_system", nr_failed_system_),
          fmt::arg("nr_failed_user", nr_failed_user_), fmt::arg("system_state", system_state_),
          fmt::arg("user_state", user_state_), fmt::arg("overall_state", overall_state_),
          fmt::arg("failed_units_list", failed_list)));
    } else {
      label_.set_tooltip_text("");
    }
  }
  ALabel::update();
}

}  // namespace waybar::modules
