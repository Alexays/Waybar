#include "modules/temperature.hpp"

#include <fmt/base.h>

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

#if defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif

waybar::modules::Temperature::Temperature(const std::string& id, const Json::Value& config)
    : ALabel(config, "temperature", id, "{temperatureC}°C", 10) {
#if defined(__FreeBSD__)
// FreeBSD uses sysctlbyname instead of read from a file
#else
  if (config_["type"].isString()) {
    auto sensor_type_string = config_["type"].asString();
    if (sensor_type_string.empty() || sensor_type_string == "temperature") {
      sensor_type_ = SensorType::TEMPERATURE;
    } else if (sensor_type_string == "fan") {
      sensor_type_ = SensorType::FAN;
    } else if (sensor_type_string == "power") {
      sensor_type_ = SensorType::POWER;
    }
  }

  auto traverseAsArray = [](const Json::Value& value, auto&& check_set_path) {
    if (value.isString())
      check_set_path(value.asString());
    else if (value.isArray())
      for (const auto& item : value)
        if (check_set_path(item.asString())) break;
  };

  if (config_["hwmon-by-name"].isString() && config_["input-filename"].isString()) {
    auto name = config_["hwmon-by-name"].asString();
    auto input_filename = config_["input-filename"].asString();
    for (const auto& entry : std::filesystem::directory_iterator("/sys/class/hwmon/")) {
      if (std::filesystem::is_directory(entry) && file_path_.empty()) {
        auto name_filepath = entry.path().string() + "/name";
        auto input_filepath = entry.path().string() + "/" + input_filename;

        if (std::filesystem::exists(name_filepath) && std::filesystem::exists(input_filepath)) {
          std::ifstream name_file(name_filepath);
          if (!name_file.is_open()) {
            throw std::runtime_error("Error: Could not open file " + name_filepath);
          }

          std::string line;
          while (std::getline(name_file, line)) {
            if (line.find(name) != std::string::npos) {
              file_path_ = input_filepath;
              break;
            }
          }
        }
      }
    }
    if (file_path_.empty()) throw std::runtime_error("Could not find hwmon by name " + name);
  }

  auto find_hwmon_by_name = [](const std::string& name) -> std::optional<std::filesystem::path> {
    for (const auto& entry : std::filesystem::directory_iterator("/sys/class/hwmon")) {
      std::ifstream f(entry.path() / "name");
      std::string hwname;
      if (f >> hwname && hwname == name) {
        return entry.path();
      }
    }
    return std::nullopt;
  };

  // ensure either hwmon-name OR old paths are used, not both
  if (config_["hwmon-name"].isString() &&
      (!config_["hwmon-path"].isNull() || !config_["hwmon-path-abs"].isNull())) {
    throw std::runtime_error(
        "hwmon-name cannot be used together with hwmon-path or hwmon-path-abs");
  }

  if (file_path_.empty()) {
    // if hwmon_path is an array, loop to find first valid item
    traverseAsArray(config_["hwmon-path"], [this](const std::string& path) {
      if (!std::filesystem::exists(path)) return false;
      file_path_ = path;
      return true;
    });
  }

  if (file_path_.empty() && config_["input-filename"].isString()) {
    // fallback to hwmon_paths-abs
    traverseAsArray(config_["hwmon-path-abs"], [this](const std::string& path) {
      if (!std::filesystem::is_directory(path)) return false;
      return std::ranges::any_of(
          std::filesystem::directory_iterator(path), [this](const auto& hwmon) {
            if (!hwmon.path().filename().string().starts_with("hwmon")) return false;
            file_path_ = hwmon.path().string() + "/" + config_["input-filename"].asString();
            return true;
          });
    });
  }

  if (file_path_.empty() && config_["hwmon-name"].isString()) {
    if (!config_["input-filename"].isString()) {
      throw std::runtime_error("hwmon-name requires input-filename to be set");
    }

    auto hwmon = find_hwmon_by_name(config_["hwmon-name"].asString());

    if (!hwmon) {
      throw std::runtime_error("hwmon-name '" + config_["hwmon-name"].asString() + "' not found");
    }

    file_path_ = hwmon->string() + "/" + config_["input-filename"].asString();
  }

  if (file_path_.empty() && sensor_type_ == SensorType::TEMPERATURE) {
    auto zone = config_["thermal-zone"].isInt() ? config_["thermal-zone"].asInt() : 0;
    file_path_ = fmt::format("/sys/class/thermal/thermal_zone{}/temp", zone);
  }

  // check if file_path_ can be used to retrieve the temperature
  std::ifstream temp(file_path_);
  if (!temp.is_open()) {
    throw std::runtime_error("Can't open " + file_path_);
  }
  if (!temp.good()) {
    temp.close();
    throw std::runtime_error("Can't read from " + file_path_);
  }
  temp.close();
#endif

  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

auto waybar::modules::Temperature::update() -> void {
  uint16_t readings = std::round(getReadings());
  auto critical = isCritical(readings);
  auto warning = isWarning(readings);
  auto format = format_;
  if (critical) {
    format = config_["format-critical"].isString() ? config_["format-critical"].asString() : format;
    label_.get_style_context()->add_class("critical");
  } else {
    label_.get_style_context()->remove_class("critical");
    if (warning) {
      format = config_["format-warning"].isString() ? config_["format-warning"].asString() : format;
      label_.get_style_context()->add_class("warning");
    } else {
      label_.get_style_context()->remove_class("warning");
    }
  }

  if (format.empty()) {
    event_box_.hide();
    return;
  }

  event_box_.show();

  auto max_reading =
      config_["critical-threshold"].isInt() ? config_["critical-threshold"].asInt() : 0;

  uint16_t power = 0;
  uint16_t fan_speed = 0;
  uint16_t temperature_c = 0;
  uint16_t temperature_f = 0;
  uint16_t temperature_k = 0;
  std::string tooltip_default = "";
  switch (sensor_type_) {
    case TEMPERATURE:
      temperature_c = readings;
      temperature_f = std::round((readings * 1.8) + 32);
      temperature_k = std::round(readings + 273.15);
      tooltip_default = "{temperatureC}°C";
      break;
    case FAN:
      fan_speed = readings;
      tooltip_default = "{fan} RPM";
      break;
    case POWER:
      power = readings;
      tooltip_default = "{power}W";
      break;
  }

  updateLabelAndTooltip(format, tooltip_default, fmt::arg("temperatureC", temperature_c),
                        fmt::arg("temperatureF", temperature_f),
                        fmt::arg("temperatureK", temperature_k),
                        fmt::arg("icon", getIcon(readings, "", max_reading)),
                        fmt::arg("fan", fan_speed), fmt::arg("power", power));

  // Call parent update
  ALabel::update();
}

float waybar::modules::Temperature::getReadings() {
#if defined(__FreeBSD__)
  if (sensor_type_ != SensorType::TEMPERATURE)
    throw std::runtime_error("Only temperature sensor reading is supported in FreeBSD");

  int temp;
  size_t size = sizeof temp;

  auto zone = config_["thermal-zone"].isInt() ? config_["thermal-zone"].asInt() : 0;

  // First, try with dev.cpu
  if ((sysctlbyname(fmt::format("dev.cpu.{}.temperature", zone).c_str(), &temp, &size, NULL, 0) ==
       0) ||
      (sysctlbyname(fmt::format("hw.acpi.thermal.tz{}.temperature", zone).c_str(), &temp, &size,
                    NULL, 0) == 0)) {
    auto temperature_c = ((float)temp - 2732) / 10;
    return temperature_c;
  }

  throw std::runtime_error(fmt::format(
      "sysctl hw.acpi.thermal.tz{}.temperature and dev.cpu.{}.temperature failed", zone, zone));
#else
  std::ifstream temp(file_path_);
  if (!temp.is_open()) {
    throw std::runtime_error("Can't open " + file_path_);
  }

  std::string line;
  if (temp.good()) {
    getline(temp, line);
  } else {
    temp.close();
    throw std::runtime_error("Can't read from " + file_path_);
  }
  temp.close();

  auto reading = std::strtol(line.c_str(), nullptr, 10);
  switch (sensor_type_) {
    case TEMPERATURE:
      return reading / 1000.0;
    case FAN:
      break;
    case POWER:
      return reading / 1000000.0;
  }

  return static_cast<float>(reading);
#endif
}

bool waybar::modules::Temperature::isWarning(uint16_t reading) {
  return config_["warning-threshold"].isInt() && reading >= config_["warning-threshold"].asInt();
}

bool waybar::modules::Temperature::isCritical(uint16_t reading) {
  return config_["critical-threshold"].isInt() && reading >= config_["critical-threshold"].asInt();
}

void waybar::modules::Temperature::suspend() { thread_.pause(); }

void waybar::modules::Temperature::resume() { thread_.resume(); }
