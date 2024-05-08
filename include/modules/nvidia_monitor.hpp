#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class NvidiaMonitor : public ALabel {
 public:
  NvidiaMonitor(const std::string&, const Json::Value&);
  ~NvidiaMonitor() override;
  auto update() -> void override;

  static const std::size_t ARGUMENTS_COUNT{5};
  using NvidiaSmiArgumentsTable = std::array<char*, ARGUMENTS_COUNT>;
  using GpuData = std::vector<std::string>;

 private:

  util::SleeperThread thread_;

  std::string nvidiaSmiCmd_{"nvidia-smi"};
  std::string nvidiaSmiArgvQuery_{
      "--query-gpu=timestamp,name,pci.bus_id,driver_version,pstate,pcie.link.gen.max,pcie.link.gen."
      "current,temperature.gpu,utilization.gpu,utilization.memory,memory.total,memory.free,memory."
      "used,power.draw,clocks.sm,clocks.mem,clocks.gr"};
  std::string nvidiaSmiArgvFormat_{"--format=csv,noheader"};
  std::filesystem::path gpuExchangeFilePath_{"gpu_data.csv"};
  std::string nvidiaSmiArgvFileName_{"--filename="};

  NvidiaSmiArgumentsTable programArgv_{nvidiaSmiCmd_.data(), nvidiaSmiArgvQuery_.data(),
                                       nvidiaSmiArgvFormat_.data(), nvidiaSmiArgvFileName_.data(),
                                       nullptr};

  void runNvidiaSmiOnce();
  static std::string generateTemporaryFileName();
  std::ifstream openExchangeFile();
  std::optional<GpuData> loadDataFromFile(std::ifstream);
  void formatBar(const GpuData&);
  void formatTooltip(const GpuData&);

};

}  // namespace waybar::modules
