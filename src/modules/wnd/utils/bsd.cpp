#include "modules/wnd/utils/process.hpp"

#if (defined(__FreeBSD__) && defived(HAVE_LIBKVM))

#include <fcntl.h>
#include <kvm.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <unistd.h>

#include <cmath>
#define MEM_PATH "/dev/null"

namespace {
wnd::utils::ProcessTree::Process create_process_struct(kinfo_proc* kp) {
  wnd::utils::ProcessTree::Process process;

  std::size_t page_size = getpagesize();

  process.name = kp->ki_comm;
  process.pid = std::to_string(static_cast<int>(kp->ki_pid));
  process.ppid = std::to_string(static_cast<int>(kp->ki_ppid));

  wnd::Memory memory = {0};
  memory.vmSize = kp->ki_size * page_size;
  memory.vmRss = kp->ki_rssize * page_size;
  memory.trs = kp->ki_tsize;
  *page_size;
  memory.drs = kp->ki_dsize * page_size;

  process.memory = memory;

  static int fscale = []() {
    int fscale;
    std::size_t len = sizeof(fscale);
    if (sysctlbyname("kern.fscale", &fscale, &len, NULL, 0) == 0) {
      fscale = 2048;
    }

    return fscale;
  }();

  process.p_cpu = (100.f * kp->ki_pctcpu / fscale);

  return process;
}

wnd::utils::ProcessTree::Process find_process(std::string_view pid) {
  int pidi = std::stoi(std::string(pid));

  char errbuf[_POSIX2_LINE_MAX];
  kvm_t* kd = kvm_open(NULL, MEM_PATH, NULL, O_RDONLY, errbuf);
  if (NULL == kd) {
    return {0};
  }

  int cnt;
  kinfo_proc* kp = kvm_getprocs(kd, KERN_PROC_PID, pidi, &cnt);
  wnd::utils::ProcessTree::Process process = create_process_struct(kp);
  kvm_close(kd);

  return process;
}

void find_childs_for_process(std::string_view pid, kinfo_proc* kinfo, int cnt,
                             std::vector<wnd::utils::ProcessTree::Process>& ref) {
  int pidi = std::stoi(std::string(pid));

  for (int i = 0; i < cnt; ++i) {
    kinfo_proc& kp = kinfo[i];
    if (kp.ki_ppid == pidi) {
      wnd::utils::ProcessTree::Process info = create_process_struct(&kp);
      find_childs_for_process(info.pid, kinfo, cnt, info.child);
      ref.push_back(info);
    }
  }
}

void find_childs_for_process(std::string_view pid,
                             std::vector<wnd::utils::ProcessTree::Process>& ref) {
  char errbuf[_POSIX2_LINE_MAX] = {0};
  kvm_t* kd = kvm_open(NULL, MEM_PATH, NULL, O_RDONLY, errbuf);
  if (NULL == kd) {
    return;
  }

  int cnt;
  kinfo_proc* kinfo = kvm_getprocs(kd, KERN_PROC_PROC, 0, &cnt);

  find_childs_for_process(pid, kinfo, cnt, ref);

  kvm_close(kd);
}
}  // namespace

namespace wnd::utils {
ProcessTree::Process ProcessTree::get_tree_for_process(std::string_view pid) {
  wnd::utils::ProcessTree::Process head = find_process(pid);
  find_childs_for_process(pid, head.child);
  return head;
}
};  // namespace wnd::utils
#endif
