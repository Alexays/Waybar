#pragma once
#include <string_view>

namespace wnd {
class ProcessCpu {
 public:
  static long get_cpu_for_process(std::string_view pid);
};
};  // namespace wnd
