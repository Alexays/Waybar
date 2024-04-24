#include <spdlog/spdlog.h>
#include <sys/sysctl.h>

#include "modules/cpu_frequency.hpp"

std::vector<float> waybar::modules::CpuFrequency::parseCpuFrequencies() {
  std::vector<float> frequencies;
  char buffer[256];
  size_t len;
  int32_t freq;
  uint32_t i = 0;

  while (true) {
    len = 4;
    snprintf(buffer, 256, "dev.cpu.%u.freq", i);
    if (sysctlbyname(buffer, &freq, &len, NULL, 0) == -1 || len <= 0) break;
    frequencies.push_back(freq);
    ++i;
  }

  if (frequencies.empty()) {
    spdlog::warn("cpu/bsd: parseCpuFrequencies failed, not found in sysctl");
    frequencies.push_back(NAN);
  }

  return frequencies;
}
