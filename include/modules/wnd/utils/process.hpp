#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "memory.hpp"

namespace wnd::utils {
class ProcessTree {
 public:
  struct Process {
    std::string name;
    std::string pid;
    std::string ppid;

    Memory memory = {0};

    float p_cpu = 0;

    std::vector<Process> child;
  };
  static Process get_tree_for_process(int pid);
};
};  // namespace wnd::utils
