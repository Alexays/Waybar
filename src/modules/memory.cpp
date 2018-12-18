#include "modules/memory.hpp"

waybar::modules::Memory::Memory(const std::string& id, const Json::Value& config)
  : ALabel(config, "{}%", 30)
{
  label_.set_name("memory");
  if (!id.empty()) {
    label_.get_style_context()->add_class(id);
  }
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

auto waybar::modules::Memory::update() -> void
{
  parseMeminfo();
  if (memtotal_ > 0 && memfree_ >= 0) {
    int used_ram_percentage = 100 * (memtotal_ - memfree_) / memtotal_;
    label_.set_markup(fmt::format(format_, used_ram_percentage));
    auto used_ram_gigabytes = (memtotal_ - memfree_) / std::pow(1024, 2);
    label_.set_tooltip_text(fmt::format("{:.{}f}Gb used", used_ram_gigabytes, 1));
    event_box_.show();
  } else {
    event_box_.hide();
  }
}

void waybar::modules::Memory::parseMeminfo()
{
  long memfree = -1, membuffer = -1, memcache = -1, memavail = -1;
  std::ifstream info(data_dir_);
  if (!info.is_open()) {
    throw std::runtime_error("Can't open " + data_dir_);
  }
  std::string line;
  while (getline(info, line)) {
    auto posDelim = line.find(":");
    if (posDelim == std::string::npos) {
      continue;
    }
    std::string name = line.substr(0, posDelim);
    long value = std::stol(line.substr(posDelim + 1));

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
    if (memtotal_ > 0 &&
      (memavail >= 0 || (memfree > -1  && membuffer > -1 && memcache > -1))) {
      break;
    }
  }
  memfree_ = memavail >= 0 ? memavail : memfree + membuffer + memcache;
}
