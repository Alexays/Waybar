#include "modules/battery.hpp"
#include <spdlog/spdlog.h>

namespace waybar::modules {

Battery::Battery(const std::string& id, const Json::Value& config)
    : ALabel(config, "battery", id, "{capacity}%", 60) {
  args_.push_back(
      Arg{"capacity", std::bind(&Battery::getCapacity, this), REVERSED_STATE | DEFAULT});
  args_.push_back(Arg{"time", std::bind(&Battery::getTimeRemaining, this)});
  args_.push_back(Arg{"", std::bind(&Battery::getTooltipText, this), TOOLTIP});
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
    uint16_t    total = 0;
    uint32_t    total_power = 0;   // μW
    uint32_t    total_energy = 0;  // μWh
    uint32_t    total_energy_full = 0;
    std::string status = "Unknown";
    for (auto const& bat : batteries_) {
      uint16_t    capacity;
      uint32_t    power_now;
      uint32_t    energy_full;
      uint32_t    energy_now;
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
      return "Charging";
    }
    return "Discharging";
  }
  return "Unknown";
}

const std::string Battery::formatTimeRemaining(float hoursRemaining) const {
  hoursRemaining = std::fabs(hoursRemaining);
  uint16_t full_hours = static_cast<uint16_t>(hoursRemaining);
  uint16_t minutes = static_cast<uint16_t>(60 * (hoursRemaining - full_hours));
  return std::to_string(full_hours) + " h " + std::to_string(minutes) + " min";
}

uint8_t Battery::getCapacity() const { return capacity_; }

const std::string Battery::getTimeRemaining() const { return formatTimeRemaining(time_remaining_); }

const std::string Battery::getTooltipText() const {
  if (time_remaining_ != 0) {
    std::string time_to = std::string("Time to ") + ((time_remaining_ > 0) ? "empty" : "full");
    return time_to + ": " + getTimeRemaining();
  }
  return status_;
}

const std::string Battery::getFormat() const {
  auto state = getState(capacity_, true);
  if (!state.empty() && config_["format-" + status_ + "-" + state].isString()) {
    return config_["format-" + status_ + "-" + state].asString();
  } else if (config_["format-" + status_].isString()) {
    return config_["format-" + status_].asString();
  } else if (!state.empty() && config_["format-" + state].isString()) {
    return config_["format-" + state].asString();
  }
  return ALabel::getFormat();
}

const std::vector<std::string> Battery::getClasses() const { return {status_}; };

auto Battery::update() -> void {
  std::tie(capacity_, time_remaining_, status_) = getInfos();
  if (status_ == "Unknown") {
    status_ = getAdapterStatus(capacity_);
  }
  std::transform(status_.begin(), status_.end(), status_.begin(), ::tolower);
  ALabel::update();
}

}  // namespace waybar::modules
