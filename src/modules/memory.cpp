#include "modules/memory.hpp"

namespace waybar::modules {

Memory::Memory(const std::string& id, const Json::Value& config)
    : ALabel(config, "memory", id, "{}%", 30) {
  args_.emplace("percentage",
                Arg{std::bind(&Memory::getPercentage, this), true, .isDefault = true});
  args_.emplace("total", Arg{std::bind(&Memory::getTotal, this)});
  args_.emplace("used", Arg{std::bind(&Memory::getUsed, this), .tooltip = true});
  args_.emplace("avail", Arg{std::bind(&Memory::getAvailable, this)});
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

auto Memory::update() -> void {
  parseMeminfo();
  ALabel::update();
}

uint8_t Memory::getPercentage() const { return 100 * (memtotal_ - memfree_) / memtotal_; }

uint16_t Memory::getTotal() const { return memtotal_ / std::pow(1024, 2); }

uint16_t Memory::getUsed() const { return (memtotal_ - memfree_) / std::pow(1024, 2); }

uint16_t Memory::getAvailable() const { return memfree_ / std::pow(1024, 2); }

void Memory::parseMeminfo() {
  int64_t       memfree = -1, membuffer = -1, memcache = -1, memavail = -1;
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

    if (name.compare("MemTotal") == 0) {
      memtotal_ = value;
    } else if (name.compare("MemAvailable") == 0) {
      memavail = value;
    } else if (name.compare("MemFree") == 0) {
      memfree = value;
    } else if (name.compare("Buffers") == 0) {
      membuffer = value;
    } else if (name.compare("Cached") == 0) {
      memcache = value;
    }
    if (memtotal_ > 0 && (memavail >= 0 || (memfree > -1 && membuffer > -1 && memcache > -1))) {
      break;
    }
  }
  memfree_ = memavail >= 0 ? memavail : memfree + membuffer + memcache;
}

}  // namespace waybar::modules
