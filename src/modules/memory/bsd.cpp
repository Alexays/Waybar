// clang-format off
#include <sys/types.h>
#include <sys/sysctl.h>
// clang-format on
#include <unistd.h>  // getpagesize

#include "modules/memory.hpp"

#if defined(__DragonFly__)
#include <sys/vmmeter.h>  // struct vmstats
#elif defined(__NetBSD__)
#include <uvm/uvm_extern.h>  // struct uvmexp_sysctl
#elif defined(__OpenBSD__)
#include <uvm/uvmexp.h>  // struct uvmexp
#endif

static uint64_t get_total_memory() {
#if defined(HW_MEMSIZE) || defined(HW_PHYSMEM64)
  uint64_t physmem;
#else
  u_long physmem;
#endif
  int mib[] = {
    CTL_HW,
#if defined(HW_MEMSIZE)
    HW_MEMSIZE,
#elif defined(HW_PHYSMEM64)
    HW_PHYSMEM64,
#else
    HW_PHYSMEM,
#endif
  };
  u_int miblen = sizeof(mib) / sizeof(mib[0]);
  size_t sz = sizeof(physmem);
  if (sysctl(mib, miblen, &physmem, &sz, NULL, 0)) {
    throw std::runtime_error("sysctl hw.physmem failed");
  }
  return physmem;
}

static uint64_t get_free_memory() {
#if defined(__DragonFly__)
  struct vmstats vms;
  size_t sz = sizeof(vms);
  if (sysctlbyname("vm.vmstats", &vms, &sz, NULL, 0)) {
    throw std::runtime_error("sysctl vm.vmstats failed");
  }
  return static_cast<uint64_t>(vms.v_free_count + vms.v_inactive_count + vms.v_cache_count) *
         getpagesize();
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
  u_int v_free_count = 0, v_inactive_count = 0, v_cache_count = 0;
  size_t sz = sizeof(u_int);
  sysctlbyname("vm.stats.vm.v_free_count", &v_free_count, &sz, NULL, 0);
  sysctlbyname("vm.stats.vm.v_inactive_count", &v_inactive_count, &sz, NULL, 0);
  sysctlbyname("vm.stats.vm.v_cache_count", &v_cache_count, &sz, NULL, 0);
  return static_cast<uint64_t>(v_free_count + v_inactive_count + v_cache_count) * getpagesize();
#elif defined(__NetBSD__) || defined(__OpenBSD__)
#ifdef VM_UVMEXP2
#undef VM_UVMEXP
#define VM_UVMEXP VM_UVMEXP2
#define uvmexp uvmexp_sysctl
#else
#define filepages vnodepages
#define execpages vtextpages
#endif
  int mib[] = {
      CTL_VM,
      VM_UVMEXP,
  };
  u_int miblen = sizeof(mib) / sizeof(mib[0]);
  struct uvmexp uvmexp;
  size_t sz = sizeof(uvmexp);
  if (sysctl(mib, miblen, &uvmexp, &sz, NULL, 0)) {
    throw std::runtime_error("sysctl vm.uvmexp failed");
  }
  return static_cast<uint64_t>(uvmexp.free + uvmexp.inactive + uvmexp.filepages +
                               uvmexp.execpages) *
         uvmexp.pagesize;
#endif
}

void waybar::modules::Memory::parseMeminfo() {
  meminfo_["MemTotal"] = get_total_memory() / 1024;
  meminfo_["MemAvailable"] = get_free_memory() / 1024;
}
