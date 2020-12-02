#include "modules/battery.hpp"

#include <spdlog/spdlog.h>

waybar::modules::Battery::Battery(const std::string& id, const Json::Value& config)
    : ALabel(config, "battery", id, "{capacity}%", 60) {
  fd_ = inotify_init1(IN_CLOEXEC);
  if (fd_ == -1) {
    throw std::runtime_error("Unable to listen batteries.");
  }

  // Watch the directory for any added or removed batteries
  global_watch = inotify_add_watch(fd_, data_dir_.c_str(), IN_CREATE | IN_DELETE);
  if (global_watch < 0) {
    throw std::runtime_error("Could not watch for battery plug/unplug");
  }

  refreshBatteries();
  worker();
}

waybar::modules::Battery::~Battery() {
  if (global_watch >= 0) {
    inotify_rm_watch(fd_, global_watch);
  }
  for (auto it = batteries_.cbegin(); it != batteries_.cend(); it++) {
    auto watch_id = (*it).second;
    if (watch_id >= 0) {
      inotify_rm_watch(fd_, watch_id);
    }
    batteries_.erase(it);
  }
  close(fd_);
}

void waybar::modules::Battery::worker() {
  thread_timer_ = [this] {
    dp.emit();
    thread_timer_.sleep_for(interval_);
  };
  thread_ = [this] {
    struct inotify_event event = {0};
    int                  nbytes = read(fd_, &event, sizeof(event));
    if (nbytes != sizeof(event) || event.mask & IN_IGNORED) {
      thread_.stop();
      return;
    }
    // TODO: don't stop timer for now since there is some bugs :?
    // thread_timer_.stop();
    dp.emit();
  };
}

void waybar::modules::Battery::refreshBatteries() {
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
          fs::exists(node.path() / "capacity") && fs::exists(node.path() / "uevent") &&
          fs::exists(node.path() / "status") && fs::exists(node.path() / "type")) {
        std::string type;
        std::ifstream(node.path() / "type") >> type;

        if (!type.compare("Battery")){
          check_map[node.path()] = true;
          auto search = batteries_.find(node.path());
          if (search == batteries_.end()) {
            // We've found a new battery save it and start listening for events
            auto event_path = (node.path() / "uevent");
            auto wd = inotify_add_watch(fd_, event_path.c_str(), IN_ACCESS);
            if (wd < 0) {
              throw std::runtime_error("Could not watch events for " + node.path().string());
            }
            batteries_[node.path()] = wd;
          }
        }
      }
      auto adap_defined = config_["adapter"].isString();
      if (((adap_defined && dir_name == config_["adapter"].asString()) || !adap_defined) &&
          fs::exists(node.path() / "online")) {
        adapter_ = node.path();
      }
    }
  } catch (fs::filesystem_error& e) {
    throw std::runtime_error(e.what());
  }
  if (batteries_.empty()) {
    if (config_["bat"].isString()) {
      throw std::runtime_error("No battery named " + config_["bat"].asString());
    }
    throw std::runtime_error("No batteries.");
  }

  // Remove any batteries that are no longer present and unwatch them
  for (auto const& check : check_map) {
    if (!check.second) {
      auto watch_id = batteries_[check.first];
      if (watch_id >= 0) {
        inotify_rm_watch(fd_, watch_id);
      }
      batteries_.erase(check.first);
    }
  }
}

const std::tuple<uint8_t, float, std::string> waybar::modules::Battery::getInfos() const {
  try {
    uint32_t    total_power = 0;   // μW
    uint32_t    total_energy = 0;  // μWh
    uint32_t    total_energy_full = 0;
    std::string status = "Unknown";
    for (auto const& item : batteries_) {
      auto bat = item.first;
      uint32_t    power_now;
      uint32_t    energy_full;
      uint32_t    energy_now;
      std::string _status;
      std::ifstream(bat / "status") >> _status;
      auto rate_path = fs::exists(bat / "current_now") ? "current_now" : "power_now";
      std::ifstream(bat / rate_path) >> power_now;
      auto now_path = fs::exists(bat / "charge_now") ? "charge_now" : "energy_now";
      std::ifstream(bat / now_path) >> energy_now;
      auto full_path = fs::exists(bat / "charge_full") ? "charge_full" : "energy_full";
      std::ifstream(bat / full_path) >> energy_full;
      if (_status != "Unknown") {
        status = _status;
      }
      total_power += power_now;
      total_energy += energy_now;
      total_energy_full += energy_full;
    }
    if (!adapter_.empty() && status == "Discharging") {
      bool online;
      std::ifstream(adapter_ / "online") >> online;
      if (online) {
        status = "Plugged";
      }
    }
    float time_remaining = 0;
    if (status == "Discharging" && total_power != 0) {
      time_remaining = (float)total_energy / total_power;
    } else if (status == "Charging" && total_power != 0) {
      time_remaining = -(float)(total_energy_full - total_energy) / total_power;
      if (time_remaining > 0.0f) {
        // If we've turned positive it means the battery is past 100% and so
        // just report that as no time remaining
        time_remaining = 0.0f;
      }
    }
    float capacity = ((float)total_energy * 100.0f / (float) total_energy_full);
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
    if (cap == 100) {
      // If we've reached 100% just mark as full as some batteries can stay
      // stuck reporting they're still charging but not yet done
      status = "Full";
    }

    return {cap, time_remaining, status};
  } catch (const std::exception& e) {
    spdlog::error("Battery: {}", e.what());
    return {0, 0, "Unknown"};
  }
}

const std::string waybar::modules::Battery::getAdapterStatus(uint8_t capacity) const {
  if (!adapter_.empty()) {
    bool online;
    std::ifstream(adapter_ / "online") >> online;
    if (capacity == 100) {
      return "Full";
    }
    if (online) {
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
  auto     format = std::string("{H} h {M} min");
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
  // Make sure we have the correct set of batteries, in case of hotplug
  // TODO: split the global watch into it's own event and only run the refresh
  //       when there's been a CREATE/DELETE event
  refreshBatteries();

  auto [capacity, time_remaining, status] = getInfos();
  if (status == "Unknown") {
    status = getAdapterStatus(capacity);
  }
  if (tooltipEnabled()) {
    std::string tooltip_text;
    if (time_remaining != 0) {
      std::string time_to = std::string("Time to ") + ((time_remaining > 0) ? "empty" : "full");
      tooltip_text = time_to + ": " + formatTimeRemaining(time_remaining);
    } else {
      tooltip_text = status;
    }
    label_.set_tooltip_text(tooltip_text);
  }
  // Transform to lowercase  and replace space with dash
  std::transform(status.begin(), status.end(), status.begin(), [](char ch) {
    return ch == ' ' ? '-' : std::tolower(ch);
  });
  auto format = format_;
  auto state = getState(capacity, true);
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
    label_.set_markup(fmt::format(format,
                                  fmt::arg("capacity", capacity),
                                  fmt::arg("icon", getIcon(capacity, icons)),
                                  fmt::arg("time", formatTimeRemaining(time_remaining))));
  }
  // Call parent update
  ALabel::update();
}
