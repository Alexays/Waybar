#include <spdlog/spdlog.h>
#include <sys/sysctl.h>

#include "modules/cpu_frequency.hpp"

std::vector<float> waybar::modules::CpuFrequency::parseCpuFrequencies() {
  std::vector<float> frequencies;
  size_t len;
  int32_t freq;

#if defined(__NetBSD__)
  char buffer[256];
  const char *freq_sysctls[] = {
#if defined(__powerpc__)
    "machdep.intrepid.frequency.current",
#endif
#if defined(__mips__)
    "machdep.loongson.frequency.current",
#endif
#if defined(__i386__) || defined(__x86_64__)
    "machdep.est.frequency.current",
    "machdep.powernow.frequency.current",
#endif
    "machdep.cpu.frequency.current",
    "machdep.frequency.current",
    NULL
  };
  uint32_t i = 0;

  while (true) {
    len = sizeof(freq);
    snprintf(buffer, 256, "machdep.cpufreq.cpu%u.current", i);
    if (sysctlbyname(buffer, &freq, &len, NULL, 0) == -1 || len <= 0) break;
    frequencies.push_back((float)freq);
    ++i;
  } 

  if (frequencies.empty()) {
    const char **s;
    for (s = freq_sysctls; *s != NULL; ++s) {
      len = sizeof(freq);
      if (sysctlbyname(*s, &freq, &len, NULL, 0) != -1 || len <= 0) {
        frequencies.push_back((float)freq);
        break;
      }
    }
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
