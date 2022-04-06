#include <spdlog/spdlog.h>
// clang-format off
#include <sys/types.h>
#include <sys/sysctl.h>
// clang-format on
#include <unistd.h>  // sysconf

#include <cmath>    // NAN
#include <cstdlib>  // malloc

#include "modules/cpu.hpp"

#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/sched.h>
#else
#include <sys/resource.h>
#endif

#if defined(__NetBSD__)
typedef uint64_t cp_time_t;
#else
typedef long cp_time_t;
#endif
#if defined(__NetBSD__) || defined(__OpenBSD__)
typedef uint64_t pcp_time_t;
#else
typedef long pcp_time_t;
#endif

std::vector<std::tuple<size_t, size_t>> waybar::modules::Cpu::parseCpuinfo() {
  cp_time_t sum_cp_time[CPUSTATES];
  size_t sum_sz = sizeof(sum_cp_time);
  int ncpu = sysconf(_SC_NPROCESSORS_CONF);
  size_t sz = CPUSTATES * (ncpu + 1) * sizeof(pcp_time_t);
  pcp_time_t *cp_time = static_cast<pcp_time_t *>(malloc(sz)), *pcp_time = cp_time;
#if defined(__NetBSD__)
  int mib[] = {
      CTL_KERN,
      KERN_CP_TIME,
  };
  if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), sum_cp_time, &sum_sz, NULL, 0)) {
    throw std::runtime_error("sysctl kern.cp_time failed");
  }
  for (int state = 0; state < CPUSTATES; state++) {
    cp_time[state] = sum_cp_time[state];
  }
  pcp_time += CPUSTATES;
  if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), pcp_time, &sz, NULL, 0)) {
    throw std::runtime_error("sysctl kern.cp_time failed");
  }
#elif defined(__OpenBSD__)
  {
    int mib[] = {
        CTL_KERN,
        KERN_CPTIME,
    };
    if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), sum_cp_time, &sum_sz, NULL, 0)) {
      throw std::runtime_error("sysctl kern.cp_time failed");
    }
  }
  for (int state = 0; state < CPUSTATES; state++) {
    cp_time[state] = sum_cp_time[state];
  }
  pcp_time = cp_time;
  sz /= ncpu + 1;
  {
    int mib[] = {
        CTL_KERN,
        KERN_CPTIME2,
        0,
    };
    for (int cpu = 0; cpu < ncpu; cpu++) {
      mib[2] = cpu;
      pcp_time += CPUSTATES;
      if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), pcp_time, &sz, NULL, 0)) {
        throw std::runtime_error("sysctl kern.cp_time2 failed");
      }
    }
  }
#else
  if (sysctlbyname("kern.cp_time", sum_cp_time, &sum_sz, NULL, 0)) {
    throw std::runtime_error("sysctl kern.cp_time failed");
  }
  for (int state = 0; state < CPUSTATES; state++) {
    cp_time[state] = sum_cp_time[state];
  }
  pcp_time += CPUSTATES;
  if (sysctlbyname("kern.cp_times", pcp_time, &sz, NULL, 0)) {
    throw std::runtime_error("sysctl kern.cp_times failed");
  }
#endif
  std::vector<std::tuple<size_t, size_t>> cpuinfo;
  for (int cpu = 0; cpu < ncpu + 1; cpu++) {
    pcp_time_t total = 0, *single_cp_time = &cp_time[cpu * CPUSTATES];
    for (int state = 0; state < CPUSTATES; state++) {
      total += single_cp_time[state];
    }
    cpuinfo.emplace_back(single_cp_time[CP_IDLE], total);
  }
  free(cp_time);
  return cpuinfo;
}

std::vector<float> waybar::modules::Cpu::parseCpuFrequencies() {
  static std::vector<float> frequencies;
  if (frequencies.empty()) {
    spdlog::warn(
        "cpu/bsd: parseCpuFrequencies is not implemented, expect garbage in {*_frequency}");
    frequencies.push_back(NAN);
  }
  return frequencies;
}
