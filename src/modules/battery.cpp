#include "modules/battery.hpp"

#include <spdlog/spdlog.h>

waybar::modules::Battery::Battery(const std::string& id, const Json::Value& config)
    : ALabel(config, "battery", id, "{capacity}%", 60) {
  battery_watch_fd_ = inotify_init1(IN_CLOEXEC);
  if (battery_watch_fd_ == -1) {
    throw std::runtime_error("Unable to listen batteries.");
  }

  global_watch_fd_ = inotify_init1(IN_CLOEXEC);
  if (global_watch_fd_ == -1) {
    throw std::runtime_error("Unable to listen batteries.");
  }

  // Watch the directory for any added or removed batteries
  global_watch = inotify_add_watch(global_watch_fd_, data_dir_.c_str(), IN_CREATE | IN_DELETE);
  if (global_watch < 0) {
    throw std::runtime_error("Could not watch for battery plug/unplug");
  }

  worker();
}

waybar::modules::Battery::~Battery() {
  std::lock_guard<std::mutex> guard(battery_list_mutex_);

  if (global_watch >= 0) {
    inotify_rm_watch(global_watch_fd_, global_watch);
  }
  close(global_watch_fd_);

  for (auto it = batteries_.cbegin(); it != batteries_.cend(); it++) {
    auto watch_id = (*it).second;
    if (watch_id >= 0) {
      inotify_rm_watch(battery_watch_fd_, watch_id);
    }
    batteries_.erase(it);
  }
  close(battery_watch_fd_);
}

void waybar::modules::Battery::worker() {
  thread_timer_ = [this] {
    // Make sure we eventually update the list of batteries even if we miss an
    // inotify event for some reason
    refreshBatteries();
    dp.emit();
    thread_timer_.sleep_for(interval_);
  };
  thread_ = [this] {
    struct inotify_event event = {0};
    int nbytes = read(battery_watch_fd_, &event, sizeof(event));
    if (nbytes != sizeof(event) || event.mask & IN_IGNORED) {
      thread_.stop();
      return;
    }
    dp.emit();
  };
  thread_battery_update_ = [this] {
    struct inotify_event event = {0};
    int nbytes = read(global_watch_fd_, &event, sizeof(event));
    if (nbytes != sizeof(event) || event.mask & IN_IGNORED) {
      thread_.stop();
      return;
    }
    refreshBatteries();
    dp.emit();
  };
}

void waybar::modules::Battery::refreshBatteries() {
  std::lock_guard<std::mutex> guard(battery_list_mutex_);
  // Mark existing list of batteries as not necessarily found
  std::map<fs::path, bool> check_map;
  for (auto const& bat : batteries_) {
    check_map[bat.first] = false;
  }

  try {
    for (auto& node : fs::directory_iterator(data_dir_)) {
      if (!fs::is_directory(node)) {
        continue;
      }
      auto dir_name = node.path().filename();
      auto bat_defined = config_["bat"].isString();
      if (((bat_defined && dir_name == config_["bat"].asString()) || !bat_defined) &&
          (fs::exists(node.path() / "capacity") || fs::exists(node.path() / "charge_now")) &&
          fs::exists(node.path() / "uevent") && fs::exists(node.path() / "status") &&
          fs::exists(node.path() / "type")) {
        std::string type;
        std::ifstream(node.path() / "type") >> type;

        if (!type.compare("Battery")) {
          check_map[node.path()] = true;
          auto search = batteries_.find(node.path());
          if (search == batteries_.end()) {
            // We've found a new battery save it and start listening for events
            auto event_path = (node.path() / "uevent");
            auto wd = inotify_add_watch(battery_watch_fd_, event_path.c_str(), IN_ACCESS);
            if (wd < 0) {
              throw std::runtime_error("Could not watch events for " + node.path().string());
            }
            batteries_[node.path()] = wd;
          }
        }
      }
      auto adap_defined = config_["adapter"].isString();
      if (((adap_defined && dir_name == config_["adapter"].asString()) || !adap_defined) &&
          (fs::exists(node.path() / "online") || fs::exists(node.path() / "status"))) {
        adapter_ = node.path();
      }
    }
  } catch (fs::filesystem_error& e) {
    throw std::runtime_error(e.what());
  }
  if (warnFirstTime_ && batteries_.empty()) {
    if (config_["bat"].isString()) {
      spdlog::warn("No battery named {0}", config_["bat"].asString());
    } else {
      spdlog::warn("No batteries.");
    }

    warnFirstTime_ = false;
  }

  // Remove any batteries that are no longer present and unwatch them
  for (auto const& check : check_map) {
    if (!check.second) {
      auto watch_id = batteries_[check.first];
      if (watch_id >= 0) {
        inotify_rm_watch(battery_watch_fd_, watch_id);
      }
      batteries_.erase(check.first);
    }
  }
}

// Unknown > Full > Not charging > Discharging > Charging
static bool status_gt(const std::string& a, const std::string& b) {
  if (a == b)
    return false;
  else if (a == "Unknown")
    return true;
  else if (a == "Full" && b != "Unknown")
    return true;
  else if (a == "Not charging" && b != "Unknown" && b != "Full")
    return true;
  else if (a == "Discharging" && b != "Unknown" && b != "Full" && b != "Not charging")
    return true;
  return false;
}

const std::tuple<uint8_t, float, std::string, float> waybar::modules::Battery::getInfos() {
  std::lock_guard<std::mutex> guard(battery_list_mutex_);

  try {
    uint32_t total_power = 0;   // μW
    bool total_power_exists = false;
    uint32_t total_energy = 0;  // μWh
    bool total_energy_exists = false;
    uint32_t total_energy_full = 0;
    bool total_energy_full_exists = false;
    uint32_t total_energy_full_design = 0;
    bool total_energy_full_design_exists = false;
    uint32_t total_capacity = 0;
    bool total_capacity_exists = false;

    std::string status = "Unknown";
    for (auto const& item : batteries_) {
      auto bat = item.first;
      std::string _status;
      std::getline(std::ifstream(bat / "status"), _status);

      // Some battery will report current and charge in μA/μAh.
      // Scale these by the voltage to get μW/μWh.

      uint32_t capacity = 0;
      bool capacity_exists = false;
      if (fs::exists(bat / "capacity")) {
       capacity_exists = true;
       std::ifstream(bat / "capacity") >> capacity;
      }

      uint32_t current_now = 0;
      bool current_now_exists = false;
      if (fs::exists(bat / "current_now")) {
       current_now_exists = true;
       std::ifstream(bat / "current_now") >> current_now;
      } else if (fs::exists(bat / "current_avg")) {
       current_now_exists = true;
       std::ifstream(bat / "current_avg") >> current_now;
      }

      uint32_t voltage_now = 0;
      bool voltage_now_exists = false;
      if (fs::exists(bat / "voltage_now")) {
       voltage_now_exists = true;
       std::ifstream(bat / "voltage_now") >> voltage_now;
      } else if (fs::exists(bat / "voltage_avg")) {
       voltage_now_exists = true;
       std::ifstream(bat / "voltage_avg") >> voltage_now;
      }

      uint32_t charge_full = 0;
      bool charge_full_exists = false;
      if (fs::exists(bat / "charge_full")) {
        charge_full_exists = true;
        std::ifstream(bat / "charge_full") >> charge_full;
      }

      uint32_t charge_full_design = 0;
      bool charge_full_design_exists = false;
      if (fs::exists(bat / "charge_full_design")) {
        charge_full_design_exists = true;
        std::ifstream(bat / "charge_full_design") >> charge_full_design;
      }

      uint32_t charge_now = 0;
      bool charge_now_exists = false;
      if (fs::exists(bat / "charge_now")) {
        charge_now_exists = true;
        std::ifstream(bat / "charge_now") >> charge_now;
      }

      uint32_t power_now = 0;
      bool power_now_exists = false;
      if (fs::exists(bat / "power_now")) {
        power_now_exists = true;
        std::ifstream(bat / "power_now") >> power_now;
      } 

      uint32_t energy_now = 0;
      bool energy_now_exists = false;
      if (fs::exists(bat / "energy_now")) {
        energy_now_exists = true;
        std::ifstream(bat / "energy_now") >> energy_now;
      }  

      uint32_t energy_full = 0;
      bool energy_full_exists = false;
      if (fs::exists(bat / "energy_full")) {
        energy_full_exists = true;
        std::ifstream(bat / "energy_full") >> energy_full;
      }

      uint32_t energy_full_design = 0;
      bool energy_full_design_exists = false;
      if (fs::exists(bat / "energy_full_design")) {
        energy_full_design_exists = true;
        std::ifstream(bat / "energy_full_design") >> energy_full_design;
      }

      if (!voltage_now_exists) {
        if (power_now_exists && current_now_exists && current_now != 0) {
          voltage_now_exists = true;
          voltage_now = 1000000 * power_now / current_now;
        } else if (energy_full_design_exists && charge_full_design_exists && charge_full_design != 0) {
          voltage_now_exists = true;
          voltage_now = 1000000 * energy_full_design / charge_full_design;
        } else if (energy_now_exists) {
          if (charge_now_exists && charge_now != 0) {
            voltage_now_exists = true;
            voltage_now = 1000000 * energy_now / charge_now;
          } else if (capacity_exists && charge_full_exists) {
            charge_now_exists = true;
            charge_now = charge_full * capacity / 100;
            if (charge_full != 0 && capacity != 0) {
              voltage_now_exists = true;
              voltage_now = 1000000 * energy_now * 100 / charge_full / capacity;
            }
          } 
        } else if (energy_full_exists) {
          if (charge_full_exists && charge_full != 0) {
            voltage_now_exists = true;
            voltage_now = 1000000 * energy_full / charge_full;
          } else if (charge_now_exists && capacity_exists) {
            if (capacity != 0) {
              charge_full_exists = true;
              charge_full = 100 * charge_now / capacity;
            }
            if (charge_now != 0) {
              voltage_now_exists = true;
              voltage_now = 10000 * energy_full * capacity / charge_now;
            }
          }
        }
      }

      if (!capacity_exists) {
        if (charge_now_exists && charge_full_exists && charge_full != 0) {
          capacity_exists = true;
          capacity = 100 * charge_now / charge_full;
        } else if (energy_now_exists && energy_full_exists && energy_full != 0) {
          capacity_exists = true;
          capacity = 100 * energy_now / energy_full;
        } else if (charge_now_exists && energy_full_exists && voltage_now_exists) {
          if (!charge_full_exists && voltage_now != 0) {
            charge_full_exists = true;
            charge_full = 1000000 * energy_full / voltage_now;
          }
          if (energy_full != 0) {
            capacity_exists = true;
            capacity = charge_now * voltage_now / 10000 / energy_full;
          }
        } else if (charge_full_exists && energy_now_exists && voltage_now_exists) {
          if (!charge_now_exists && voltage_now != 0) {
            charge_now_exists = true;
            charge_now = 1000000 * energy_now / voltage_now;
          }
          if (voltage_now != 0 && charge_full != 0) {
            capacity_exists = true;
            capacity = 100 * 1000000 * energy_now / voltage_now / charge_full;
          }
        }
      }

      if (!energy_now_exists && voltage_now_exists) {
        if (charge_now_exists) {
          energy_now_exists = true;
          energy_now = charge_now * voltage_now / 1000000;
        } else if (capacity_exists && charge_full_exists) {
          charge_now_exists = true;
          charge_now = capacity * charge_full / 100;
          energy_now_exists = true;
          energy_now = voltage_now * capacity * charge_full / 1000000 / 100;
        } else if (capacity_exists && energy_full) {
          if (voltage_now != 0) {
            charge_full_exists = true;
            charge_full = 1000000 * energy_full / voltage_now;
            charge_now_exists = true;
            charge_now = capacity * 10000 * energy_full / voltage_now;
          }
          energy_now_exists = true;
          energy_now = capacity * energy_full / 100;
        } 
      }

      if (!energy_full_exists && voltage_now_exists) {
        if (charge_full_exists) {
          energy_full_exists = true;
          energy_full = charge_full * voltage_now / 1000000;
        } else if (charge_now_exists && capacity_exists && capacity != 0) {
          charge_full_exists = true;
          charge_full = 100 * charge_now / capacity;
          energy_full_exists = true;
          energy_full = charge_now * voltage_now / capacity / 10000;
        } else if (capacity_exists && energy_now) {
          if (voltage_now != 0) {
            charge_now_exists = true;
            charge_now = 1000000 * energy_now / voltage_now;
          }
          if (capacity != 0) {
            energy_full_exists = true;
            energy_full = 100 * energy_now / capacity;
            if (voltage_now != 0) {
              charge_full_exists = true;
              charge_full = 100 * 1000000 * energy_now / voltage_now / capacity;
            }
          }
        }
      }

      if (!power_now_exists && voltage_now_exists && current_now_exists) {
        power_now_exists = true;
        power_now = voltage_now * current_now / 1000000;
      }

      if (!energy_full_design_exists && voltage_now_exists && charge_full_design_exists) {
        energy_full_design_exists = true;
        energy_full_design = voltage_now * charge_full_design / 1000000;
      }

      // Show the "smallest" status among all batteries
      if (status_gt(status, _status))
        status = _status;

      if (power_now_exists) {
        total_power_exists = true;
        total_power += power_now;
      }
      if (energy_now_exists) {
        total_energy_exists = true;
        total_energy += energy_now;
      }
      if (energy_full_exists) {
        total_energy_full_exists = true;
        total_energy_full += energy_full;
      }
      if (energy_full_design_exists) {
        total_energy_full_design_exists = true;
        total_energy_full_design += energy_full_design;
      }
      if (capacity_exists) {
        total_capacity_exists = true;
        total_capacity += capacity;
      }
    }

    if (!adapter_.empty() && status == "Discharging") {
      bool online;
      std::string current_status;
      std::ifstream(adapter_ / "online") >> online;
      std::getline(std::ifstream(adapter_ / "status"), current_status);
      if (online && current_status != "Discharging")
        status = "Plugged";
    }

    float time_remaining{0.0f};
    if (status == "Discharging" && total_power_exists && total_energy_exists) {
      if (total_power != 0)
        time_remaining = (float)total_energy / total_power;
    } else if (status == "Charging" && total_energy_exists && total_energy_full_exists && total_power_exists) {
      if (total_power != 0)
        time_remaining = -(float)(total_energy_full - total_energy) / total_power;
      // If we've turned positive it means the battery is past 100% and so just report that as no time remaining
      if (time_remaining > 0.0f)
        time_remaining = 0.0f;
    }

    float calculated_capacity{0.0f};
    if (total_capacity_exists) {
      if (total_capacity > 0.0f)
        calculated_capacity = (float)total_capacity;
      else if (total_energy_full_exists && total_energy_exists) {
        if (total_energy_full > 0.0f)
          calculated_capacity = ((float)total_energy * 100.0f / (float)total_energy_full);
      }
    }
    
    // Handle design-capacity
    if ((config_["design-capacity"].isBool() ? config_["design-capacity"].asBool() : false) && total_energy_exists && total_energy_full_design_exists) {
      if (total_energy_full_design > 0.0f)
        calculated_capacity = ((float)total_energy * 100.0f / (float)total_energy_full_design);
    }

    // Handle full-at
    if (config_["full-at"].isUInt()) {
      auto full_at = config_["full-at"].asUInt();
      if (full_at < 100) 
        calculated_capacity = 100.f * calculated_capacity / full_at;
    }

    // Handle it gracefully by clamping at 100%
    // This can happen when the battery is calibrating and goes above 100%
    if (calculated_capacity > 100.f)  
      calculated_capacity = 100.f;
    
    uint8_t cap = round(calculated_capacity);
    // If we've reached 100% just mark as full as some batteries can stay stuck reporting they're still charging but not yet done
    if (cap == 100 && status == "Charging") 
      status = "Full";

    return {cap, time_remaining, status, total_power / 1e6};
  } catch (const std::exception& e) {
    spdlog::error("Battery: {}", e.what());
    return {0, 0, "Unknown", 0};
  }
}

const std::string waybar::modules::Battery::getAdapterStatus(uint8_t capacity) const {
  if (!adapter_.empty()) {
    bool online;
    std::string status;
    std::ifstream(adapter_ / "online") >> online;
    std::getline(std::ifstream(adapter_ / "status"), status);
    if (capacity == 100) {
      return "Full";
    }
    if (online && status != "Discharging") {
      return "Plugged";
    }
    return "Discharging";
  }
  return "Unknown";
}

const std::string waybar::modules::Battery::formatTimeRemaining(float hoursRemaining) {
  hoursRemaining = std::fabs(hoursRemaining);
  uint16_t full_hours = static_cast<uint16_t>(hoursRemaining);
  uint16_t minutes = static_cast<uint16_t>(60 * (hoursRemaining - full_hours));
  auto format = std::string("{H} h {M} min");
  if (full_hours == 0 && minutes == 0) {
    // Migh as well not show "0h 0min"
    return "";
  }
  if (config_["format-time"].isString()) {
    format = config_["format-time"].asString();
  }
  return fmt::format(format, fmt::arg("H", full_hours), fmt::arg("M", minutes));
}

auto waybar::modules::Battery::update() -> void {
  if (batteries_.empty()) {
    event_box_.hide();
    return;
  }
  auto [capacity, time_remaining, status, power] = getInfos();
  if (status == "Unknown") {
    status = getAdapterStatus(capacity);
  }
  auto status_pretty = status;
  // Transform to lowercase  and replace space with dash
  std::transform(status.begin(), status.end(), status.begin(),
                 [](char ch) { return ch == ' ' ? '-' : std::tolower(ch); });
  auto format = format_;
  auto state = getState(capacity, true);
  auto time_remaining_formatted = formatTimeRemaining(time_remaining);
  if (tooltipEnabled()) {
    std::string tooltip_text_default;
    std::string tooltip_format = "{timeTo}";
    if (time_remaining != 0) {
      std::string time_to = std::string("Time to ") + ((time_remaining > 0) ? "empty" : "full");
      tooltip_text_default = time_to + ": " + time_remaining_formatted;
    } else {
      tooltip_text_default = status_pretty;
    }
    if (!state.empty() && config_["tooltip-format-" + status + "-" + state].isString()) {
      tooltip_format = config_["tooltip-format-" + status + "-" + state].asString();
    } else if (config_["tooltip-format-" + status].isString()) {
      tooltip_format = config_["tooltip-format-" + status].asString();
    } else if (!state.empty() && config_["tooltip-format-" + state].isString()) {
      tooltip_format = config_["tooltip-format-" + state].asString();
    } else if (config_["tooltip-format"].isString()) {
      tooltip_format = config_["tooltip-format"].asString();
    }
    label_.set_tooltip_text(fmt::format(tooltip_format, fmt::arg("timeTo", tooltip_text_default),
                                        fmt::arg("capacity", capacity),
                                        fmt::arg("time", time_remaining_formatted)));
  }
  if (!old_status_.empty()) {
    label_.get_style_context()->remove_class(old_status_);
  }
  label_.get_style_context()->add_class(status);
  old_status_ = status;
  if (!state.empty() && config_["format-" + status + "-" + state].isString()) {
    format = config_["format-" + status + "-" + state].asString();
  } else if (config_["format-" + status].isString()) {
    format = config_["format-" + status].asString();
  } else if (!state.empty() && config_["format-" + state].isString()) {
    format = config_["format-" + state].asString();
  }
  if (format.empty()) {
    event_box_.hide();
  } else {
    event_box_.show();
    auto icons = std::vector<std::string>{status + "-" + state, status, state};
    label_.set_markup(fmt::format(format, fmt::arg("capacity", capacity), fmt::arg("power", power),
                                  fmt::arg("icon", getIcon(capacity, icons)),
                                  fmt::arg("time", time_remaining_formatted)));
  }
  // Call parent update
  ALabel::update();
}
