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
  int used_ram_percentage = 100 * (memtotal_ - memfree_) / memtotal_;
  label_.set_text(fmt::format(format_, used_ram_percentage));
  auto used_ram_gigabytes = (memtotal_ - memfree_) / std::pow(1024, 2);
  label_.set_tooltip_text(fmt::format("{:.{}f}Gb used", used_ram_gigabytes, 1));
}

void waybar::modules::Memory::parseMeminfo()
{
  int memtotal, memfree, memavail, membuffer, memcache;
  FILE* info = fopen("/proc/meminfo","r");
  if(fscanf (info, "MemTotal: %d kB MemFree: %d kB Buffers: %d kB Cached: %d kB",&memtotal, &memfree, &membuffer, &memcache) < 4) { // Old meminfo format 
    fclose(info);
    info = fopen("/proc/meminfo","r"); 
    if(fscanf(info, "MemTotal: %d kB MemFree: %d kB MemAvailable: %d kB Buffers: %d kB Cached: %d kB",&memtotal, &memfree, &memavail, &membuffer, &memcache) < 5) { // Current meminfo format
      memtotal_ = -1;
      memfree_ = -1;
    } else {
      memtotal_ = memtotal;
      memfree_ = memavail;
    }
  } else {
    memtotal_ = memtotal;
    memfree_ = memfree + (membuffer + memcache);
  }
  fclose(info);
}
