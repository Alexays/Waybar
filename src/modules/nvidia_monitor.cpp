#include "modules/nvidia_monitor.hpp"

#include <fmt/core.h>
#include <json/value.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

waybar::modules::NvidiaMonitor::NvidiaMonitor(const std::string& id, const Json::Value& config)
    : ALabel(config, "nvidia_monitor", id, "{gpuTemp}°C", 10),
      gpuExchangeFilePath_{config_["data-exchange-path"].asString()},
      nvidiaSmiArgvFileName_{"--filename=" + gpuExchangeFilePath_.string()} {
  if (!config_["data-exchange-path"].isString()) {
    gpuExchangeFilePath_ =
        std::filesystem::temp_directory_path() / "waybar" / generateTemporaryFileName();
    nvidiaSmiArgvFileName_ = "--filename=" + gpuExchangeFilePath_.string();
    std::filesystem::create_directory(gpuExchangeFilePath_.parent_path());
    std::ofstream exchangeFile(gpuExchangeFilePath_);
    exchangeFile.close();
    spdlog::debug("[{}]: The exchange path not found in config.\nCreating exchange file in: {}",
                  name_, gpuExchangeFilePath_.string());
    programArgv_[3] = nvidiaSmiArgvFileName_.data();
  }

  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

waybar::modules::NvidiaMonitor::~NvidiaMonitor() {
  std::filesystem::remove_all(gpuExchangeFilePath_.parent_path());
}

auto waybar::modules::NvidiaMonitor::update() -> void {
  runNvidiaSmiOnce();
  auto data = loadDataFromFile(openExchangeFile());

  if (data.has_value()) {
    auto gpuData = data.value();

    formatBar(gpuData);
    formatTooltip(gpuData);
  }

  ALabel::update();
}

void waybar::modules::NvidiaMonitor::runNvidiaSmiOnce() {
  auto pid = fork();
  if (pid == 0) {
    for (auto& item : programArgv_) {
      spdlog::debug("[{}]: {}", name_, item);
    }
    auto isError = execvp(nvidiaSmiCmd_.c_str(), programArgv_.data());
    if (isError < 0) {
      perror("execve");
      throw std::runtime_error("Can't run nvidia-smi.");
    }
  }
}

std::string waybar::modules::NvidiaMonitor::generateTemporaryFileName() {
  std::size_t h1 = std::hash<std::string>{}(
      std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
  std::size_t h2 = std::hash<std::string>{}(std::to_string(static_cast<int>(getpid())));
  return std::to_string(h1 ^ (h2 << 1));
}

std::ifstream waybar::modules::NvidiaMonitor::openExchangeFile() {
  auto gpuDataFile{std::ifstream(gpuExchangeFilePath_)};
  if (gpuDataFile.is_open()) {
    gpuDataFile.seekg(0);
    spdlog::debug("[{}]: File is open", name_);
    return gpuDataFile;
  }
  spdlog::error("Invalid file {}", gpuExchangeFilePath_.string());
  spdlog::error("{}", nvidiaSmiArgvFileName_);
  throw std::runtime_error("Can't open the exchange file");
}

std::optional<waybar::modules::NvidiaMonitor::GpuData>
waybar::modules::NvidiaMonitor::loadDataFromFile(std::ifstream gpuDataFile) {
  GpuData gpuData;
  std::string buff;
  while (std::getline(gpuDataFile, buff, ',')) {
    spdlog::debug("[{}]: Data: {}", name_, buff);
    gpuData.emplace_back(buff);
  }

  if (gpuData.empty()) {
    spdlog::warn("[{}]: Exchange file empty", name_);
    return std::nullopt;
  }

  return std::make_optional(gpuData);
}

void waybar::modules::NvidiaMonitor::formatBar(const GpuData& gpuData) {
  auto barText = fmt::format(
      fmt::runtime(format_), fmt::arg("name", gpuData[1]), fmt::arg("busId", gpuData[2]),
      fmt::arg("driverVersion", gpuData[3]), fmt::arg("powerState", gpuData[4]),
      fmt::arg("pcieLinkMax", gpuData[5]), fmt::arg("pcieLinkCurr", gpuData[6]),
      fmt::arg("gpuTemp", gpuData[7]), fmt::arg("gpuUtilization", gpuData[8]),
      fmt::arg("memUtilization", gpuData[9]), fmt::arg("memTotal", gpuData[10]),
      fmt::arg("memFree", gpuData[11]), fmt::arg("memUsed", gpuData[12]),
      fmt::arg("powerDraw", gpuData[13]), fmt::arg("clocksSm", gpuData[14]),
      fmt::arg("clocksMem", gpuData[15]), fmt::arg("clocksGr", gpuData[16]));

  label_.set_markup(barText);
}

void waybar::modules::NvidiaMonitor::formatTooltip(const GpuData& gpuData) {
  std::string tooltipFormat;
  auto defaultTooltipText = fmt::format(
      fmt::runtime(format_), fmt::arg("name", gpuData[1]), fmt::arg("busId", gpuData[2]),
      fmt::arg("driverVersion", gpuData[3]), fmt::arg("powerState", gpuData[4]),
      fmt::arg("pcieLinkMax", gpuData[5]), fmt::arg("pcieLinkCurr", gpuData[6]),
      fmt::arg("gpuTemp", gpuData[7]), fmt::arg("gpuUtilization", gpuData[8]),
      fmt::arg("memUtilization", gpuData[9]), fmt::arg("memTotal", gpuData[10]),
      fmt::arg("memFree", gpuData[11]), fmt::arg("memUsed", gpuData[12]),
      fmt::arg("powerDraw", gpuData[13]), fmt::arg("clocksSm", gpuData[14]),
      fmt::arg("clocksMem", gpuData[15]), fmt::arg("clocksGr", gpuData[16]));

  if (tooltipEnabled()) {
    label_.set_tooltip_markup(defaultTooltipText);
    if (config_["tooltip-format"].isString()) {
      tooltipFormat = config_["tooltip-format"].asString();
    }
    if (tooltipFormat.empty()) {
      label_.set_tooltip_markup(defaultTooltipText);
    } else {
      auto tooltipText = fmt::format(
          fmt::runtime(tooltipFormat), fmt::arg("name", gpuData[1]), fmt::arg("busId", gpuData[2]),
          fmt::arg("driverVersion", gpuData[3]), fmt::arg("powerState", gpuData[4]),
          fmt::arg("pcieLinkMax", gpuData[5]), fmt::arg("pcieLinkCurr", gpuData[6]),
          fmt::arg("gpuTemp", gpuData[7]), fmt::arg("gpuUtilization", gpuData[8]),
          fmt::arg("memUtilization", gpuData[9]), fmt::arg("memTotal", gpuData[10]),
          fmt::arg("memFree", gpuData[11]), fmt::arg("memUsed", gpuData[12]),
          fmt::arg("powerDraw", gpuData[13]), fmt::arg("clocksSm", gpuData[14]),
          fmt::arg("clocksMem", gpuData[15]), fmt::arg("clocksGr", gpuData[16]));

      label_.set_tooltip_markup(tooltipText);
    }
  }
}
