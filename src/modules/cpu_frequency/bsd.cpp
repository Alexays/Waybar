#include <spdlog/spdlog.h>
#include <sys/sysctl.h>

#include "modules/cpu_frequency.hpp"

std::vector<float> waybar::modules::CpuFrequency::parseCpuFrequencies() {
  std::vector<float> frequencies;
  size_t len;
  int32_t freq;

#if defined(__NetBSD__)
  if (sysctlbyname("machdep.cpu.frequency.current", &freq, &len, NULL, 0) == 0) {
    frequencies.push_back((float)freq);
  }
#elif defined(__OpenBSD__)
  int getMhz[] = {CTL_HW, HW_CPUSPEED};
  len = sizeof(freq);
  sysctl(getMhz, 2, &freq, &len, NULL, 0);
  frequencies.push_back((float)freq);
#else
  char buffer[256];
  uint32_t i = 0;
  while (true) {
    len = 4;
    snprintf(buffer, 256, "dev.cpu.%u.freq", i);
    if (sysctlbyname(buffer, &freq, &len, NULL, 0) == -1 || len <= 0) break;
    frequencies.push_back(freq);
    ++i;
  }
#endif

  if (frequencies.empty()) {
    spdlog::warn("cpu/bsd: parseCpuFrequencies failed, not found in sysctl");
    frequencies.push_back(NAN);
  }

  return frequencies;
}
