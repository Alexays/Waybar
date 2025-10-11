#include "modules/wwan.hpp"
#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdio>
#include "ModemManager.h"
#include "libmm-glib.h"

// In the 80000 version of fmt library authors decided to optimize imports
// and moved declarations required for fmt::dynamic_format_arg_store in new
// header fmt/args.h
#if (FMT_VERSION >= 80000)
#include <fmt/args.h>
#else
#include <fmt/core.h>
#endif

namespace {
  using namespace waybar::util;
  constexpr const char *DEFAULT_FORMAT = "{state}";
}  // namespace


waybar::modules::Wwan::Wwan(const std::string& id, const Json::Value& config)
: ALabel(config, "wwan", id, "{}", 5) {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };

  GError* error = nullptr;
  connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
  if (error) {
    g_error_free(error);
    return;
  }


  manager = mm_manager_new_sync(
    connection,
    G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
    nullptr,  // cancellable
    &error
  );

  if (error) {
    spdlog::error("Failed to create ModemManager proxy: " + std::string(error->message));
    g_error_free(error);
    g_object_unref(connection);
    return;
  }

  updateCurrentModem();

  if (config_["hide-disconnected"].isBool()) {
    hideDisconnected = config_["hide-disconnected"].asBool();
  }

}

const void waybar::modules::Wwan::updateCurrentModem(){
  // get 1st modem (or modem with the specified hardware path)

  GList* modems = g_dbus_object_manager_get_objects(G_DBUS_OBJECT_MANAGER(manager));
  if (!modems) {
    return;
  }

  for (GList* m = modems; m; m = g_list_next(m)) {
    MMObject* modem_obj = MM_OBJECT(m->data);

    MMModem* modem = mm_object_get_modem(modem_obj);

    if (config_["path"].isString()) {
      std::string path = config_["path"].asString();
      std::string other = mm_modem_dup_physdev(modem);

      if(path != other) {
        g_object_unref(modem);
        modem = nullptr;
        continue;
      }
    }

    if (config_["imei"].isString()) {
      std::string imei = config_["imei"].asString();
      std::string other = mm_modem_dup_equipment_identifier(modem);

      if(imei != other) {
        g_object_unref(modem);
        modem = nullptr;
        continue;
      }
    }

    current_modem = modem;
    g_list_free_full(modems, g_object_unref);
    return;
  }
  // no modems found
  current_modem = nullptr;
  g_list_free_full(modems, g_object_unref);
}

const std::string getModemStateString(MMModem* modem) {
  switch (mm_modem_get_state(modem)) {
    case MMModemState::MM_MODEM_STATE_FAILED:
      return "Failed";
    case MMModemState::MM_MODEM_STATE_INITIALIZING:
      return "Initializing";
    case MMModemState::MM_MODEM_STATE_LOCKED:
      return "Locked";
    case MMModemState::MM_MODEM_STATE_DISABLED:
      return "Disabled";
    case MMModemState::MM_MODEM_STATE_DISABLING:
      return "Disabling";
    case MMModemState::MM_MODEM_STATE_ENABLED:
      return "Enabled";
    case MMModemState::MM_MODEM_STATE_ENABLING:
      return "Enabling";
    case MMModemState::MM_MODEM_STATE_SEARCHING:
      return "Searching";
    case MMModemState::MM_MODEM_STATE_REGISTERED:
      return "Registered";
    case MMModemState::MM_MODEM_STATE_DISCONNECTING:
      return "Disconnecting";
    case MMModemState::MM_MODEM_STATE_CONNECTING:
      return "Connecting";
    case MMModemState::MM_MODEM_STATE_CONNECTED:
      return "Connected";
    default:
      return "Unknown";
  }
}

const std::string getModemStateFormatString(MMModem* modem) {
  switch (mm_modem_get_state(modem)) {
    case MMModemState::MM_MODEM_STATE_FAILED:
      return "failed";
    case MMModemState::MM_MODEM_STATE_LOCKED:
      return "locked";
    case MMModemState::MM_MODEM_STATE_DISABLED:
      return "disabled";
    case MMModemState::MM_MODEM_STATE_ENABLED:
      return "enabled";
    case MMModemState::MM_MODEM_STATE_SEARCHING:
      return "searching";
    case MMModemState::MM_MODEM_STATE_REGISTERED:
      return "registered";
    case MMModemState::MM_MODEM_STATE_CONNECTED:
      return "connected";
    default:
      return "";
  }
}

const std::string getAccessTechnologiesString(MMModem* modem) {
  MMModemAccessTechnology technologies = mm_modem_get_access_technologies(modem);

  std::string buffer;

  if(technologies & MMModemAccessTechnology::MM_MODEM_ACCESS_TECHNOLOGY_POTS)
    buffer += "POTS, ";
  if(technologies & MMModemAccessTechnology::MM_MODEM_ACCESS_TECHNOLOGY_GSM)
    buffer += "GSM, ";
  if(technologies & MMModemAccessTechnology::MM_MODEM_ACCESS_TECHNOLOGY_GSM_COMPACT)
    buffer += "GSM Compact, ";
  if(technologies & MMModemAccessTechnology::MM_MODEM_ACCESS_TECHNOLOGY_GPRS)
    buffer += "GPRS, ";
  if(technologies & MMModemAccessTechnology::MM_MODEM_ACCESS_TECHNOLOGY_EDGE)
    buffer += "EDGE, ";
  if(technologies & MMModemAccessTechnology::MM_MODEM_ACCESS_TECHNOLOGY_UMTS)
    buffer += "UMTS, ";
  if(technologies & MMModemAccessTechnology::MM_MODEM_ACCESS_TECHNOLOGY_HSDPA)
    buffer += "HSDPA, ";
  if(technologies & MMModemAccessTechnology::MM_MODEM_ACCESS_TECHNOLOGY_HSUPA)
    buffer += "HSUPA, ";
  if(technologies & MMModemAccessTechnology::MM_MODEM_ACCESS_TECHNOLOGY_HSPA)
    buffer += "HSPA, ";
  if(technologies & MMModemAccessTechnology::MM_MODEM_ACCESS_TECHNOLOGY_HSPA_PLUS)
    buffer += "HSPA+, ";
  if(technologies & MMModemAccessTechnology::MM_MODEM_ACCESS_TECHNOLOGY_1XRTT)
    buffer += "1xRTT, ";
  if(technologies & MMModemAccessTechnology::MM_MODEM_ACCESS_TECHNOLOGY_EVDO0)
    buffer += "EVDO0, ";
  if(technologies & MMModemAccessTechnology::MM_MODEM_ACCESS_TECHNOLOGY_EVDOA)
    buffer += "EVDOA, ";
  if(technologies & MMModemAccessTechnology::MM_MODEM_ACCESS_TECHNOLOGY_EVDOB)
    buffer += "EVDOB, ";
  if(technologies & MMModemAccessTechnology::MM_MODEM_ACCESS_TECHNOLOGY_LTE)
    buffer += "LTE, ";
  if(technologies & MMModemAccessTechnology::MM_MODEM_ACCESS_TECHNOLOGY_5GNR)
    buffer += "5GNR, ";
  if(technologies & MMModemAccessTechnology::MM_MODEM_ACCESS_TECHNOLOGY_LTE_CAT_M)
    buffer += "LTE CAT-M, ";
  if(technologies & MMModemAccessTechnology::MM_MODEM_ACCESS_TECHNOLOGY_LTE_NB_IOT)
    buffer += "LTE NB-IoT, ";

  if (buffer.length() > 2) {
    buffer.erase(buffer.length()-2);
  }
  return buffer;

}

const std::string stringifyModemMode(MMModemMode mmode) {

  switch (mmode) {
    case MM_MODEM_MODE_CS:
      return "CS";
    case MM_MODEM_MODE_2G:
      return "2G";
    case MM_MODEM_MODE_3G:
      return "3G";
    case MM_MODEM_MODE_4G:
      return "4G";
    case MM_MODEM_MODE_5G:
      return "5G";
    default:
      return "No Service";
  }
}

const MMModemModeCombination getCurrentModes(MMModem* modem) {

  MMModemModeCombination combination;

  mm_modem_get_current_modes(modem, &combination.allowed, &combination.preferred);

  return combination;
}

const std::string getPreferredModeString(MMModem* modem) {

  MMModemModeCombination combination = getCurrentModes(modem);

  return stringifyModemMode(combination.preferred);
}


const std::string getCurrentModesString(MMModem* modem) {

  MMModemModeCombination combination = getCurrentModes(modem);

  // TODO: fix
  return stringifyModemMode(combination.allowed);
}

const std::string getPowerStateString(MMModem* modem) {
  MMModemPowerState state = mm_modem_get_power_state(modem);

  switch (state) {
    case MM_MODEM_POWER_STATE_ON:
      return "On";
    case MM_MODEM_POWER_STATE_OFF:
      return "Off";
    case MM_MODEM_POWER_STATE_LOW:
      return "Low";
    default:
      return "Unknown";
  }
}

const std::string getOperatorNameString(MMModem* modem) {

  MMSim* sim = mm_modem_get_sim_sync(modem, nullptr, nullptr);

  if (sim == nullptr) {
    return "No SIM";
  }

  std::string name = mm_sim_dup_operator_name(sim);
  g_object_unref(sim);
  return name;
}

auto waybar::modules::Wwan::update() -> void {
  if (current_modem == nullptr) {
    event_box_.set_visible(false);
    updateCurrentModem();
    return;
  }

  if (hideDisconnected) {
    event_box_.set_visible(false);

    return;
  }

  // Show the module
  if (!event_box_.get_visible()) event_box_.set_visible(true);

  std::string tooltip_format;

  if (!alt_) {
    std::string state = getModemStateFormatString(current_modem);
    if (!state_.empty() && label_.get_style_context()->has_class(state_)) {
      label_.get_style_context()->remove_class(state_);
    }
    if (config_["format-" + state].isString()) {
      default_format_ = config_["format-" + state].asString();
    } else if (config_["format"].isString()) {
      default_format_ = config_["format"].asString();
    } else {
      default_format_ = DEFAULT_FORMAT;
    }
    if (config_["tooltip-format-" + state].isString()) {
      tooltip_format = config_["tooltip-format-" + state].asString();
    }
    if (!label_.get_style_context()->has_class(state)) {
      label_.get_style_context()->add_class(state);
    }
    format_ = default_format_;
    state_ = state;
  }


  auto format = format_;

  fmt::dynamic_format_arg_store<fmt::format_context> store;
  store.push_back(fmt::arg("state", getModemStateString(current_modem)));

  store.push_back(fmt::arg("access_technologies", getAccessTechnologiesString(current_modem)));

  store.push_back(fmt::arg("current_modes", getCurrentModesString(current_modem)));
  store.push_back(fmt::arg("preferred_mode", getPreferredModeString(current_modem)));
  //store.push_back(fmt::arg("supported_modes", getSupportedModesString()));

  //store.push_back(fmt::arg("current_bands", getCurrentBandsString()));
  //store.push_back(fmt::arg("supported_bands", getSupportedBandsString()));

  store.push_back(fmt::arg("signal_quality", mm_modem_get_signal_quality(current_modem, nullptr)));
  store.push_back(fmt::arg("power_state", getPowerStateString(current_modem)));
  store.push_back(fmt::arg("imei", mm_modem_dup_equipment_identifier(current_modem)));

  store.push_back(fmt::arg("operator_name", getOperatorNameString(current_modem)));

  auto text = fmt::vformat(format, store);

  if (tooltipEnabled()) {
    if (tooltip_format.empty() && config_["tooltip-format"].isString()) {
      tooltip_format = config_["tooltip-format"].asString();
    }
    if (!tooltip_format.empty()) {
      auto tooltip_text = fmt::vformat(tooltip_format, store);
      if (label_.get_tooltip_text() != tooltip_text) {
        label_.set_tooltip_markup(tooltip_text);
      }
    } else if (label_.get_tooltip_text() != text) {
      label_.set_tooltip_markup(text);
    }
  }
  label_.set_markup(text);
  // Call parent update
  ALabel::update();
}

waybar::modules::Wwan::~Wwan() {
  g_object_unref(current_modem);
  g_object_unref(manager);
  g_object_unref(connection);
}
