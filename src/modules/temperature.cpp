#include "modules/temperature.hpp"

#include <filesystem>

namespace waybar::modules {

Temperature::Temperature(const std::string& id, const Json::Value& config)
    : ALabel(config, "temperature", id, "{temperatureC}°C", "{temperatureC}", 10) {
  if (config_["hwmon-path"].isString()) {
    file_path_ = config_["hwmon-path"].asString();
  } else if (config_["hwmon-path-abs"].isString() && config_["input-filename"].isString()) {
    file_path_ = (*std::filesystem::directory_iterator(config_["hwmon-path-abs"].asString()))
                     .path()
                     .u8string() +
                 "/" + config_["input-filename"].asString();
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

auto Temperature::update(std::string format,
                         fmt::dynamic_format_arg_store<fmt::format_context>& args) -> void {
  // Add default arg
  auto temperature = getTemperature();
  uint16_t temperature_c = std::round(temperature);
  args.push_back(temperature_c);
  auto temperatureCArg = fmt::arg("temperatureC", temperature_c);
  args.push_back(std::cref(temperatureCArg));
  auto temp = temperature_c;

  bool critical = false;
  std::string state;

  // If temperature_c is used, so use celcius, otherwise use farenheit
  if (ALabel::hasFormat("") || ALabel::hasFormat("temperatureC")) {
    state = getState(temperature_c);
    critical = isCritical(temperature_c);
  }

  if (ALabel::hasFormat("temperatureF")) {
    auto temperature_f = std::round(temperature * 1.8 + 32);
    auto temperatureFArg = fmt::arg("temperatureF", temperature_f);
    if (!ALabel::hasFormat("") && !ALabel::hasFormat("temperatureC") &&
        !ALabel::hasFormat("temperatureK")) {
        temp = temperature_f;
        state = getState(temperature_f);
        critical = isCritical(temperature_f);
      }
    args.push_back(std::cref(temperatureFArg));
  }

  if (ALabel::hasFormat("temperatureK")) {
    auto temperature_k = std::round(temperature * 273.15);
    auto temperatureKArg = fmt::arg("temperatureK", temperature_k);
    if (!ALabel::hasFormat("") && !ALabel::hasFormat("temperatureC") &&
        !ALabel::hasFormat("temperatureF")) {
      temp = temperature_k;
      state = getState(temperature_k);
      critical = isCritical(temperature_k);
    }
    args.push_back(std::cref(temperatureKArg));
  }

  if (critical) {
    if (config_["format-critical"].isString()) {
      format = config_["format-critical"].asString();
    }
    label_.get_style_context()->add_class("critical");
  } else {
    label_.get_style_context()->remove_class("critical");
  }

  if (ALabel::hasFormat("icon")) {
    auto max_temp =
        config_["critical-threshold"].isInt() ? config_["critical-threshold"].asInt() : 0;
    auto icon = getIcon(temp, state, max_temp);
    auto iconArg = fmt::arg("icon", icon);
    args.push_back(std::cref(iconArg));
  }

  // Call parent update
  ALabel::update(format, args);
}

float Temperature::getTemperature() const {
  std::ifstream temp(file_path_);
  if (!temp.is_open()) {
    throw std::runtime_error("Can't open " + file_path_);
  }
  std::string line;
  if (temp.good()) {
    getline(temp, line);
  }
  temp.close();
  auto temperature_c = std::strtol(line.c_str(), nullptr, 10) / 1000.0;
  return temperature_c;
}

bool Temperature::isCritical(uint16_t temperature_c) const {
  return config_["critical-threshold"].isInt() &&
         temperature_c >= config_["critical-threshold"].asInt();
}

}  // namespace waybar::modules
