#include "modules/battery.hpp"

#include <algorithm>
#if defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif
#include <spdlog/spdlog.h>

#include <iostream>
waybar::modules::Battery::Battery(const std::string& id, const Bar& bar, const Json::Value& config)
    : ALabel(config, "battery", id, "{capacity}%", 60), bar_(bar) {
#if defined(__linux__)
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
#endif
  worker();
}

waybar::modules::Battery::~Battery() {
#if defined(__linux__)
  std::lock_guard<std::mutex> guard(battery_list_mutex_);

  if (global_watch >= 0) {
    inotify_rm_watch(global_watch_fd_, global_watch);
  }
  close(global_watch_fd_);

  for (auto it = batteries_.cbegin(), next_it = it; it != batteries_.cend(); it = next_it) {
    ++next_it;
    auto watch_id = (*it).second;
    if (watch_id >= 0) {
      inotify_rm_watch(battery_watch_fd_, watch_id);
    }
    batteries_.erase(it);
  }
  close(battery_watch_fd_);
#endif
}

void waybar::modules::Battery::worker() {
#if defined(__FreeBSD__)
  thread_timer_ = [this] {
    dp.emit();
    thread_timer_.sleep_for(interval_);
  };
#else
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
#endif
}

void waybar::modules::Battery::refreshBatteries() {
#if defined(__linux__)
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
      bool bat_compatibility = config_["bat-compatibility"].asBool();
      if (((bat_defined && dir_name == config_["bat"].asString()) || !bat_defined) &&
          (fs::exists(node.path() / "capacity") || fs::exists(node.path() / "charge_now")) &&
          fs::exists(node.path() / "uevent") &&
          (fs::exists(node.path() / "status") || bat_compatibility) &&
          fs::exists(node.path() / "type")) {
        std::string type;
        std::ifstream(node.path() / "type") >> type;

        if (!type.compare("Battery")) {
          // Ignore non-system power supplies unless explicitly requested
          if (!bat_defined && fs::exists(node.path() / "scope")) {
            std::string scope;
            std::ifstream(node.path() / "scope") >> scope;
            if (g_ascii_strcasecmp(scope.data(), "device") == 0) {
              continue;
            }
          }

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
#endif
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

std::tuple<uint8_t, float, std::string, float, uint16_t, float>
waybar::modules::Battery::getInfos() {
  std::lock_guard<std::mutex> guard(battery_list_mutex_);

  try {
#if defined(__FreeBSD__)
    /* Allocate state of battery units reported via ACPI. */
    int battery_units = 0;
    size_t battery_units_size = sizeof battery_units;
    if (sysctlbyname("hw.acpi.battery.units", &battery_units, &battery_units_size, NULL, 0) != 0) {
      throw std::runtime_error("sysctl hw.acpi.battery.units failed");
    }

    if (battery_units < 0) {
      throw std::runtime_error("No battery units");
    }

    int capacity;
    size_t size_capacity = sizeof capacity;
    if (sysctlbyname("hw.acpi.battery.life", &capacity, &size_capacity, NULL, 0) != 0) {
      throw std::runtime_error("sysctl hw.acpi.battery.life failed");
    }
    int time;
    size_t size_time = sizeof time;
    if (sysctlbyname("hw.acpi.battery.time", &time, &size_time, NULL, 0) != 0) {
      throw std::runtime_error("sysctl hw.acpi.battery.time failed");
    }
    int rate;
    size_t size_rate = sizeof rate;
    if (sysctlbyname("hw.acpi.battery.rate", &rate, &size_rate, NULL, 0) != 0) {
      throw std::runtime_error("sysctl hw.acpi.battery.rate failed");
    }

    auto status = getAdapterStatus(capacity);
    // Handle full-at
    if (config_["full-at"].isUInt()) {
      auto full_at = config_["full-at"].asUInt();
      if (full_at < 100) {
        capacity = 100.f * capacity / full_at;
      }
    }
    if (capacity > 100.f) {
      // This can happen when the battery is calibrating and goes above 100%
      // Handle it gracefully by clamping at 100%
      capacity = 100.f;
    }
    uint8_t cap = round(capacity);
    if (cap == 100 && status == "Plugged") {
      // If we've reached 100% just mark as full as some batteries can stay
      // stuck reporting they're still charging but not yet done
      status = "Full";
    }

    // spdlog::info("{} {} {} {}", capacity,time,status,rate);
    return {capacity, time / 60.0, status, rate, 0, 0.0F};

#elif defined(__linux__)
    uint32_t total_power = 0;  // μW
    bool total_power_exists = false;
    uint32_t total_energy = 0;  // μWh
    bool total_energy_exists = false;
    uint32_t total_energy_full = 0;
    bool total_energy_full_exists = false;
    uint32_t total_energy_full_design = 0;
    bool total_energy_full_design_exists = false;
    uint32_t total_capacity = 0;
    bool total_capacity_exists = false;
    uint32_t time_to_empty_now = 0;
    bool time_to_empty_now_exists = false;
    uint32_t time_to_full_now = 0;
    bool time_to_full_now_exists = false;

    uint32_t largestDesignCapacity = 0;
    uint16_t mainBatCycleCount = 0;
    float mainBatHealthPercent = 0.0F;

    std::string status = "Unknown";
    for (auto const& item : batteries_) {
      auto bat = item.first;
      std::string _status;

      /* Check for adapter status if battery is not available */
      if (!std::ifstream(bat / "status")) {
        std::getline(std::ifstream(adapter_ / "status"), _status);
      } else {
        std::getline(std::ifstream(bat / "status"), _status);
      }

      // Some battery will report current and charge in μA/μAh.
      // Scale these by the voltage to get μW/μWh.

      uint32_t current_now = 0;
      bool current_now_exists = false;
      if (fs::exists(bat / "current_now")) {
        current_now_exists = true;
        std::ifstream(bat / "current_now") >> current_now;
      } else if (fs::exists(bat / "current_avg")) {
        current_now_exists = true;
        std::ifstream(bat / "current_avg") >> current_now;
      }

      if (fs::exists(bat / "time_to_empty_now")) {
        time_to_empty_now_exists = true;
        std::ifstream(bat / "time_to_empty_now") >> time_to_empty_now;
      }

      if (fs::exists(bat / "time_to_full_now")) {
        time_to_full_now_exists = true;
        std::ifstream(bat / "time_to_full_now") >> time_to_full_now;
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

      uint16_t cycleCount = 0;
      if (fs::exists(bat / "cycle_count")) {
        std::ifstream(bat / "cycle_count") >> cycleCount;
      }
      if (charge_full_design >= largestDesignCapacity) {
        largestDesignCapacity = charge_full_design;

        if (cycleCount > mainBatCycleCount) {
          mainBatCycleCount = cycleCount;
        }

        if (charge_full_exists && charge_full_design_exists) {
          float batHealthPercent = ((float)charge_full / charge_full_design) * 100;
          if (mainBatHealthPercent == 0.0F || batHealthPercent < mainBatHealthPercent) {
            mainBatHealthPercent = batHealthPercent;
          }
        } else if (energy_full_exists && energy_full_design_exists) {
          float batHealthPercent = ((float)energy_full / energy_full_design) * 100;
          if (mainBatHealthPercent == 0.0F || batHealthPercent < mainBatHealthPercent) {
            mainBatHealthPercent = batHealthPercent;
          }
        }
      }

      uint32_t capacity = 0;
      bool capacity_exists = false;
      if (charge_now_exists && charge_full_exists && charge_full != 0) {
        capacity_exists = true;
        capacity = 100 * (uint64_t)charge_now / (uint64_t)charge_full;
      } else if (energy_now_exists && energy_full_exists && energy_full != 0) {
        capacity_exists = true;
        capacity = 100 * (uint64_t)energy_now / (uint64_t)energy_full;
      } else if (fs::exists(bat / "capacity")) {
        capacity_exists = true;
        std::ifstream(bat / "capacity") >> capacity;
      }

      if (!voltage_now_exists) {
        if (power_now_exists && current_now_exists && current_now != 0) {
          voltage_now_exists = true;
          voltage_now = 1000000 * (uint64_t)power_now / (uint64_t)current_now;
        } else if (energy_full_design_exists && charge_full_design_exists &&
                   charge_full_design != 0) {
          voltage_now_exists = true;
          voltage_now = 1000000 * (uint64_t)energy_full_design / (uint64_t)charge_full_design;
        } else if (energy_now_exists) {
          if (charge_now_exists && charge_now != 0) {
            voltage_now_exists = true;
            voltage_now = 1000000 * (uint64_t)energy_now / (uint64_t)charge_now;
          } else if (capacity_exists && charge_full_exists) {
            charge_now_exists = true;
            charge_now = (uint64_t)charge_full * (uint64_t)capacity / 100;
            if (charge_full != 0 && capacity != 0) {
              voltage_now_exists = true;
              voltage_now =
                  1000000 * (uint64_t)energy_now * 100 / (uint64_t)charge_full / (uint64_t)capacity;
            }
          }
        } else if (energy_full_exists) {
          if (charge_full_exists && charge_full != 0) {
            voltage_now_exists = true;
            voltage_now = 1000000 * (uint64_t)energy_full / (uint64_t)charge_full;
          } else if (charge_now_exists && capacity_exists) {
            if (capacity != 0) {
              charge_full_exists = true;
              charge_full = 100 * (uint64_t)charge_now / (uint64_t)capacity;
            }
            if (charge_now != 0) {
              voltage_now_exists = true;
              voltage_now =
                  10000 * (uint64_t)energy_full * (uint64_t)capacity / (uint64_t)charge_now;
            }
          }
        }
      }

      if (!capacity_exists) {
        if (charge_now_exists && energy_full_exists && voltage_now_exists) {
          if (!charge_full_exists && voltage_now != 0) {
            charge_full_exists = true;
            charge_full = 1000000 * (uint64_t)energy_full / (uint64_t)voltage_now;
          }
          if (energy_full != 0) {
            capacity_exists = true;
            capacity = (uint64_t)charge_now * (uint64_t)voltage_now / 10000 / (uint64_t)energy_full;
          }
        } else if (charge_full_exists && energy_now_exists && voltage_now_exists) {
          if (!charge_now_exists && voltage_now != 0) {
            charge_now_exists = true;
            charge_now = 1000000 * (uint64_t)energy_now / (uint64_t)voltage_now;
          }
          if (voltage_now != 0 && charge_full != 0) {
            capacity_exists = true;
            capacity = 100 * 1000000 * (uint64_t)energy_now / (uint64_t)voltage_now /
                       (uint64_t)charge_full;
          }
        }
      }

      if (!energy_now_exists && voltage_now_exists) {
        if (charge_now_exists) {
          energy_now_exists = true;
          energy_now = (uint64_t)charge_now * (uint64_t)voltage_now / 1000000;
        } else if (capacity_exists && charge_full_exists) {
          charge_now_exists = true;
          charge_now = (uint64_t)capacity * (uint64_t)charge_full / 100;
          energy_now_exists = true;
          energy_now =
              (uint64_t)voltage_now * (uint64_t)capacity * (uint64_t)charge_full / 1000000 / 100;
        } else if (capacity_exists && energy_full) {
          if (voltage_now != 0) {
            charge_full_exists = true;
            charge_full = 1000000 * (uint64_t)energy_full / (uint64_t)voltage_now;
            charge_now_exists = true;
            charge_now = (uint64_t)capacity * 10000 * (uint64_t)energy_full / (uint64_t)voltage_now;
          }
          energy_now_exists = true;
          energy_now = (uint64_t)capacity * (uint64_t)energy_full / 100;
        }
      }

      if (!energy_full_exists && voltage_now_exists) {
        if (charge_full_exists) {
          energy_full_exists = true;
          energy_full = (uint64_t)charge_full * (uint64_t)voltage_now / 1000000;
        } else if (charge_now_exists && capacity_exists && capacity != 0) {
          charge_full_exists = true;
          charge_full = 100 * (uint64_t)charge_now / (uint64_t)capacity;
          energy_full_exists = true;
          energy_full = (uint64_t)charge_now * (uint64_t)voltage_now / (uint64_t)capacity / 10000;
        } else if (capacity_exists && energy_now) {
          if (voltage_now != 0) {
            charge_now_exists = true;
            charge_now = 1000000 * (uint64_t)energy_now / (uint64_t)voltage_now;
          }
          if (capacity != 0) {
            energy_full_exists = true;
            energy_full = 100 * (uint64_t)energy_now / (uint64_t)capacity;
            if (voltage_now != 0) {
              charge_full_exists = true;
              charge_full =
                  100 * 1000000 * (uint64_t)energy_now / (uint64_t)voltage_now / (uint64_t)capacity;
            }
          }
        }
      }

      if (!power_now_exists && voltage_now_exists && current_now_exists) {
        power_now_exists = true;
        power_now = (uint64_t)voltage_now * (uint64_t)current_now / 1000000;
      }

      if (!energy_full_design_exists && voltage_now_exists && charge_full_design_exists) {
        energy_full_design_exists = true;
        energy_full_design = (uint64_t)voltage_now * (uint64_t)charge_full_design / 1000000;
      }

      // Show the "smallest" status among all batteries
      if (status_gt(status, _status)) status = _status;

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

    // Give `Plugged` higher priority over `Not charging`.
    // So in a setting where TLP is used, `Plugged` is shown when the threshold is reached
    if (!adapter_.empty() && (status == "Discharging" || status == "Not charging")) {
      bool online;
      std::string current_status;
      std::ifstream(adapter_ / "online") >> online;
      std::getline(std::ifstream(adapter_ / "status"), current_status);
      if (online && current_status != "Discharging") status = "Plugged";
    }

    float time_remaining{0.0f};
    if (status == "Discharging" && time_to_empty_now_exists) {
      if (time_to_empty_now != 0) time_remaining = (float)time_to_empty_now / 3600.0f;
    } else if (status == "Discharging" && total_power_exists && total_energy_exists) {
      if (total_power != 0) time_remaining = (float)total_energy / total_power;
    } else if (status == "Charging" && time_to_full_now_exists) {
      if (time_to_full_now_exists && (time_to_full_now != 0))
        time_remaining = -(float)time_to_full_now / 3600.0f;
      // If we've turned positive it means the battery is past 100% and so just report that as no
      // time remaining
      if (time_remaining > 0.0f) time_remaining = 0.0f;
    } else if (status == "Charging" && total_energy_exists && total_energy_full_exists &&
               total_power_exists) {
      if (total_power != 0)
        time_remaining = -(float)(total_energy_full - total_energy) / total_power;
      // If we've turned positive it means the battery is past 100% and so just report that as no
      // time remaining
      if (time_remaining > 0.0f) time_remaining = 0.0f;
    }

    float calculated_capacity{0.0f};
    if (total_capacity_exists) {
      if (total_capacity > 0.0f)
        calculated_capacity = (float)total_capacity / batteries_.size();
      else if (total_energy_full_exists && total_energy_exists) {
        if (total_energy_full > 0.0f)
          calculated_capacity = ((float)total_energy * 100.0f / (float)total_energy_full);
      }
    }

    // Handle weighted-average
    if ((config_["weighted-average"].isBool() ? config_["weighted-average"].asBool() : false) &&
        total_energy_exists && total_energy_full_exists) {
      if (total_energy_full > 0.0f)
        calculated_capacity = ((float)total_energy * 100.0f / (float)total_energy_full);
    }

    // Handle design-capacity
    if ((config_["design-capacity"].isBool() ? config_["design-capacity"].asBool() : false) &&
        total_energy_exists && total_energy_full_design_exists) {
      if (total_energy_full_design > 0.0f)
        calculated_capacity = ((float)total_energy * 100.0f / (float)total_energy_full_design);
    }

    // Handle full-at
    if (config_["full-at"].isUInt()) {
      auto full_at = config_["full-at"].asUInt();
      if (full_at < 100) calculated_capacity = 100.f * calculated_capacity / full_at;
    }

    // Handle it gracefully by clamping at 100%
    // This can happen when the battery is calibrating and goes above 100%
    if (calculated_capacity > 100.f) calculated_capacity = 100.f;

    uint8_t cap = round(calculated_capacity);
    // If we've reached 100% just mark as full as some batteries can stay stuck reporting they're
    // still charging but not yet done
    if (cap == 100 && status == "Charging") status = "Full";

    return {
        cap, time_remaining, status, total_power / 1e6, mainBatCycleCount, mainBatHealthPercent};
#endif
  } catch (const std::exception& e) {
    spdlog::error("Battery: {}", e.what());
    return {0, 0, "Unknown", 0, 0, 0.0f};
  }
}

const std::string waybar::modules::Battery::getAdapterStatus(uint8_t capacity) const {
#if defined(__FreeBSD__)
  int state;
  size_t size_state = sizeof state;
  if (sysctlbyname("hw.acpi.battery.state", &state, &size_state, NULL, 0) != 0) {
    throw std::runtime_error("sysctl hw.acpi.battery.state failed");
  }
  bool online = state == 2;
  std::string status{"Unknown"};  // TODO: add status in FreeBSD
  {
#else
  if (!adapter_.empty()) {
    bool online;
    std::string status;
    std::ifstream(adapter_ / "online") >> online;
    std::getline(std::ifstream(adapter_ / "status"), status);
#endif
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
  std::string zero_pad_minutes = fmt::format("{:02d}", minutes);
  return fmt::format(fmt::runtime(format), fmt::arg("H", full_hours), fmt::arg("M", minutes),
                     fmt::arg("m", zero_pad_minutes));
}

auto waybar::modules::Battery::update() -> void {
#if defined(__linux__)
  if (batteries_.empty()) {
    event_box_.hide();
    return;
  }
#endif
  auto [capacity, time_remaining, status, power, cycles, health] = getInfos();
  if (status == "Unknown") {
    status = getAdapterStatus(capacity);
  }
  auto status_pretty = status;
  // Transform to lowercase  and replace space with dash
  std::transform(status.begin(), status.end(), status.begin(),
                 [](char ch) { return ch == ' ' ? '-' : std::tolower(ch); });
  auto format = format_;
  auto state = getState(capacity, true);
  setBarClass(state);
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
    label_.set_tooltip_text(
        fmt::format(fmt::runtime(tooltip_format), fmt::arg("timeTo", tooltip_text_default),
                    fmt::arg("power", power), fmt::arg("capacity", capacity),
                    fmt::arg("time", time_remaining_formatted), fmt::arg("cycles", cycles),
                    fmt::arg("health", fmt::format("{:.3}", health))));
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
    label_.set_markup(fmt::format(
        fmt::runtime(format), fmt::arg("capacity", capacity), fmt::arg("power", power),
        fmt::arg("icon", getIcon(capacity, icons)), fmt::arg("time", time_remaining_formatted),
        fmt::arg("cycles", cycles), fmt::arg("health", fmt::format("{:.3}", health))));
  }
  // Call parent update
  ALabel::update();
}

void waybar::modules::Battery::setBarClass(std::string& state) {
  auto classes = bar_.window.get_style_context()->list_classes();
  const std::string prefix = "battery-";

  auto old_class_it = std::find_if(classes.begin(), classes.end(), [&prefix](auto classname) {
    return classname.rfind(prefix, 0) == 0;
  });

  auto new_class = prefix + state;

  // If the bar doesn't have any `battery-` class
  if (old_class_it == classes.end()) {
    if (!state.empty()) {
      bar_.window.get_style_context()->add_class(new_class);
    }
    return;
  }

  auto old_class = *old_class_it;

  // If the bar has a `battery-` class,
  // but `state` is empty
  if (state.empty()) {
    bar_.window.get_style_context()->remove_class(old_class);
    return;
  }

  // If the bar has a `battery-` class,
  // and `state` is NOT empty
  if (old_class != new_class) {
    bar_.window.get_style_context()->remove_class(old_class);
    bar_.window.get_style_context()->add_class(new_class);
  }
}
