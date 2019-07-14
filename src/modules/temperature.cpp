#include "modules/temperature.hpp"

namespace waybar::modules {

Temperature::Temperature(const std::string& id, const Json::Value& config)
    : ALabel(config, "temperature", id, "{temperatureC}°C", 10) {
  args_.push_back(Arg{"temperatureC",
                      std::bind(&Temperature::getCelcius, this),
                      STATE | DEFAULT,
                      .state_threshold = config_["critical-threshold"].isInt()
                                             ? config_["critical-threshold"].asInt()
                                             : 0});
  args_.push_back(Arg{"temperatureF", std::bind(&Temperature::getFahrenheit, this)});
  if (config_["hwmon-path"].isString()) {
    file_path_ = config_["hwmon-path"].asString();
  } else {
    auto zone = config_["thermal-zone"].isInt() ? config_["thermal-zone"].asInt() : 0;
    file_path_ = fmt::format("/sys/class/thermal/thermal_zone{}/temp", zone);
  }
  std::ifstream temp(file_path_);
  if (!temp.is_open()) {
    throw std::runtime_error("Can't open " + file_path_);
  }
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

uint16_t Temperature::getCelcius() const { return std::round(temperature_); }

uint16_t Temperature::getFahrenheit() const { return std::round(temperature_ * 1.8 + 32); }

bool Temperature::isCritical() const {
  return config_["critical-threshold"].isInt() &&
         temperature_ >= config_["critical-threshold"].asInt();
}

const std::string Temperature::getFormat() const {
  return isCritical() && config_["format-critical"].isString()
             ? config_["format-critical"].asString()
             : ALabel::getFormat();
}

const std::vector<std::string> Temperature::getClasses() const {
  if (isCritical()) {
    return {"critical"};
  }
  return ALabel::getClasses();
}

auto Temperature::update() -> void {
  temperature_ = getTemperature();
  ALabel::update();
}

uint16_t Temperature::getTemperature() const {
  std::ifstream temp(file_path_);
  if (!temp.is_open()) {
    throw std::runtime_error("Can't open " + file_path_);
  }
  std::string line;
  if (temp.good()) {
    getline(temp, line);
  }
  temp.close();
  return std::strtol(line.c_str(), nullptr, 10) / 1000.0;
}

}  // namespace waybar::modules
