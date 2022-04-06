#include "modules/memory.hpp"

static unsigned zfsArcSize() {
  std::ifstream zfs_arc_stats{"/proc/spl/kstat/zfs/arcstats"};

  if (zfs_arc_stats.is_open()) {
    std::string name;
    std::string type;
    unsigned long data{0};

    std::string line;
    while (std::getline(zfs_arc_stats, line)) {
      std::stringstream(line) >> name >> type >> data;

      if (name == "size") {
        return data / 1024;  // convert to kB
      }
    }
  }

  return 0;
}

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
    int64_t value = std::stol(line.substr(posDelim + 1));
    meminfo_[name] = value;
  }

  meminfo_["zfs_size"] = zfsArcSize();
}
