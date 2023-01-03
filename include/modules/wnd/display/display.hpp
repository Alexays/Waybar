#pragma once
#include "modules/wnd/utils/process.hpp"

namespace wnd::display {
class Display {
 private:
  std::string _format;

 public:
  Display(std::string format);
  std::string show(const utils::ProcessTree::Process&);
  std::string show_head(const utils::ProcessTree::Process&);
};
};  // namespace wnd::display
