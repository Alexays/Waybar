#pragma once
#include <string_view>

#include "modules/wnd/utils/memory.hpp"

namespace wnd {
class ProcessMemory {
 public:
  static wnd::Memory get_memory_for_process(std::string_view pid);
};
};  // namespace wnd
