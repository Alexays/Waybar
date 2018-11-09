#include "modules/memory.hpp"

waybar::modules::Memory::Memory(const Json::Value& config)
  : ALabel(config, "{}%")
{
  label_.set_name("memory");
  uint32_t interval = config_["interval"].isUInt() ? config_["interval"].asUInt() : 30;
  thread_ = [this, interval] {
    dp.emit();
    thread_.sleep_for(chrono::seconds(interval));
  };
}

auto waybar::modules::Memory::update() -> void
{
  parseMeminfo();
  if(memtotal_ > 0 && memfree_ >= 0) {
    int used_ram_percentage = 100 * (memtotal_ - memfree_) / memtotal_;
    label_.set_text(fmt::format(format_, used_ram_percentage));
    auto used_ram_gigabytes = (memtotal_ - memfree_) / std::pow(1024, 2);
    label_.set_tooltip_text(fmt::format("{:.{}f}Gb used", used_ram_gigabytes, 1));
    label_.show();
  } else {
    label_.hide();
  }
}

void waybar::modules::Memory::parseMeminfo()
{
  long memtotal = -1, memfree = -1, membuffer = -1, memcache = -1, memavail = -1;
  int count = 0;
  std::string line;
  std::ifstream info("/proc/meminfo");
  if(info.is_open()) {
    while(getline(info, line)) {
      auto posDelim = line.find(":");
      std::string name = line.substr(0, posDelim);
      long value = std::stol(line.substr(posDelim + 1));

      if(name.compare("MemTotal") == 0) {
        memtotal = value;
        count++;
      } else if(name.compare("MemAvailable") == 0) {
        memavail = value;
        count++;
      } else if(name.compare("MemFree") == 0) {
        memfree = value;
        count++;
      } else if(name.compare("Buffers") == 0) {
        membuffer = value;
        count++;
      } else if(name.compare("Cached") == 0) {
        memcache = value;
        count++;
      }
      if (count >= 5 || (count >= 4 && memavail >= -1)) {
        info.close();
      }
    }
  } else {
    throw std::runtime_error("Can't open /proc/meminfo");
  }
  memtotal_ = memtotal;
  if(memavail >= 0) {
    memfree_ = memavail;
  } else {
    memfree_ = memfree + (membuffer + memcache);
  }
}
