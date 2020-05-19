#include "modules/memory.hpp"

void waybar::modules::Memory::parseMeminfo() {
  const std::string data_dir_ = "/proc/meminfo";
  std::ifstream info(data_dir_);
  if (!info.is_open()) {
    throw std::runtime_error("Can't open " + data_dir_);
  }
  std::string line;
  while (getline(info, line)) {
    auto posDelim = line.find(':');
    if (posDelim == std::string::npos) {
      continue;
    }

    std::string name = line.substr(0, posDelim);
    int64_t     value = std::stol(line.substr(posDelim + 1));
    meminfo_[name] = value;
  }
}
