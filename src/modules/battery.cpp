#include "modules/battery.hpp"

#include <spdlog/spdlog.h>

namespace waybar::modules {

Battery::Battery(const std::string& id, const Json::Value& config)
    : ALabel(config, "battery", id, "{capacity}%", "{time}", 60) {
  getBatteries();
  fd_ = inotify_init1(IN_CLOEXEC);
  if (fd_ == -1) {
    throw std::runtime_error("Unable to listen batteries.");
  }
  for (auto const& bat : batteries_) {
    auto wd = inotify_add_watch(fd_, (bat / "uevent").c_str(), IN_ACCESS);
    if (wd != -1) {
      wds_.push_back(wd);
    }
  }
  worker();
}

Battery::~Battery() {
  for (auto wd : wds_) {
    inotify_rm_watch(fd_, wd);
  }
  close(fd_);
}

void Battery::worker() {
  thread_timer_ = [this] {
    dp.emit();
    thread_timer_.sleep_for(interval_);
  };
  thread_ = [this] {
    struct inotify_event event = {0};
    int nbytes = read(fd_, &event, sizeof(event));
    if (nbytes != sizeof(event) || event.mask & IN_IGNORED) {
      thread_.stop();
      return;
    }
    // TODO: don't stop timer for now since there is some bugs :?
    // thread_timer_.stop();
    dp.emit();
  };
}

void Battery::getBatteries() {
  try {
    for (auto& node : fs::directory_iterator(data_dir_)) {
      if (!fs::is_directory(node)) {
        continue;
      }
      auto dir_name = node.path().filename();
      auto bat_defined = config_["bat"].isString();
      if (((bat_defined && dir_name == config_["bat"].asString()) || !bat_defined) &&
          fs::exists(node.path() / "capacity") && fs::exists(node.path() / "uevent") &&
          fs::exists(node.path() / "status")) {
        batteries_.push_back(node.path());
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
}

const std::tuple<uint8_t, float, std::string> Battery::getInfos() const {
  try {
    uint16_t total = 0;
    uint32_t total_power = 0;   // μW
    uint32_t total_energy = 0;  // μWh
    uint32_t total_energy_full = 0;
    std::string status = "Unknown";
    for (auto const& bat : batteries_) {
      uint16_t capacity;
      uint32_t power_now;
      uint32_t energy_full;
      uint32_t energy_now;
      std::string _status;
      std::ifstream(bat / "capacity") >> capacity;
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
      total += capacity;
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
    }
    uint16_t capacity = total / batteries_.size();
    // Handle full-at
    if (config_["full-at"].isUInt()) {
      auto full_at = config_["full-at"].asUInt();
      if (full_at < 100) {
        capacity = static_cast<uint16_t>(
            (static_cast<float>(capacity) / static_cast<float>(full_at)) * 100);
        if (capacity > full_at) {
          capacity = full_at;
        }
      }
    }
    return {capacity, time_remaining, status};
  } catch (const std::exception& e) {
    spdlog::error("Battery: {}", e.what());
    return {0, 0, "Unknown"};
  }
}

const std::string Battery::getAdapterStatus(uint8_t capacity) const {
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

const std::string Battery::formatTimeRemaining(float hoursRemaining) const {
  hoursRemaining = std::fabs(hoursRemaining);
  uint16_t full_hours = static_cast<uint16_t>(hoursRemaining);
  uint16_t minutes = static_cast<uint16_t>(60 * (hoursRemaining - full_hours));
  auto format = std::string("{H} h {M} min");
  if (config_["format-time"].isString()) {
    format = config_["format-time"].asString();
  }
  return fmt::format(format, fmt::arg("H", full_hours), fmt::arg("M", minutes));
}

auto Battery::update(std::string format,
                     fmt::dynamic_format_arg_store<fmt::format_context>& args,
                     std::string tooltipFormat) -> void {
  // Remove older status
  if (!status_.empty()) {
    label_.get_style_context()->remove_class(status_);
  }

  auto [capacity, time_remaining, status_] = getInfos();
  // If status is `Unknown` fallback to adapter status
  if (status_ == "Unknown") {
    status_ = getAdapterStatus(capacity);
  }

  // Add status class
  label_.get_style_context()->add_class(status_);

  // Add capacity format arg
  auto capacityArg = fmt::arg("capacity", capacity);
  args.push_back(std::cref(capacityArg));

  // Add icon based on capacity and state
  auto state = getState(capacity, true);

  if (ALabel::hasFormat("icon")) {
    auto icon = getIcon(capacity, state);
    auto iconArg = fmt::arg("icon", icon);
    args.push_back(std::cref(iconArg));
  }

  // Add time remaining
  auto timeRemaining = formatTimeRemaining(time_remaining);
  auto timeArg = fmt::arg("time", timeRemaining);
  args.push_back(std::cref(timeArg));

  // Tooltip
  // TODO: tooltip-format based on args
  tooltipFormat = status_;
  if (time_remaining != 0) {
    std::string time_to = std::string("Time to ") + ((time_remaining > 0) ? "empty" : "full");
    tooltipFormat = time_to + ": " + timeRemaining;
  }

  // Transform to lowercase and replace space with dash
  auto status = status_;
  std::transform(status.begin(), status.end(), status.begin(), [](char ch) {
    return ch == ' ' ? '-' : std::tolower(ch);
  });

  auto formatTmp = getFormat("format", status, state);
  if (!formatTmp.empty()) {
    format = formatTmp;
  }

  // Call parent update
  ALabel::update(format, args, tooltipFormat);
}

}  // namespace waybar::modules
