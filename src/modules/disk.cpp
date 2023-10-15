#include "modules/disk.hpp"

using namespace waybar::util;

waybar::modules::Disk::Disk(const std::string& id, const Json::Value& config)
    : ALabel(config, "disk", id, "{}%", 30), path_("/") {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
  if (config["path"].isString()) {
    path_ = config["path"].asString();
  }
  if (config["unit"].isString()) {
    unit_ = config["unit"].asString();
  }
}

auto waybar::modules::Disk::update() -> void {
  struct statvfs /* {
      unsigned long  f_bsize;    // filesystem block size
      unsigned long  f_frsize;   // fragment size
      fsblkcnt_t     f_blocks;   // size of fs in f_frsize units
      fsblkcnt_t     f_bfree;    // # free blocks
      fsblkcnt_t     f_bavail;   // # free blocks for unprivileged users
      fsfilcnt_t     f_files;    // # inodes
      fsfilcnt_t     f_ffree;    // # free inodes
      fsfilcnt_t     f_favail;   // # free inodes for unprivileged users
      unsigned long  f_fsid;     // filesystem ID
      unsigned long  f_flag;     // mount flags
      unsigned long  f_namemax;  // maximum filename length
  }; */
      stats;
  int err = statvfs(path_.c_str(), &stats);

  /* Conky options
    fs_bar - Bar that shows how much space is used
    fs_free - Free space on a file system
    fs_free_perc - Free percentage of space
    fs_size - File system size
    fs_used - File system used space
  */

  if (err != 0) {
    event_box_.hide();
    return;
  }

  float specific_free, specific_used, specific_total, divisor;

  divisor = calc_specific_divisor(unit_);
  specific_free = (stats.f_bavail * stats.f_frsize) / divisor;
  specific_used = ((stats.f_blocks - stats.f_bfree) * stats.f_frsize) / divisor;
  specific_total = (stats.f_blocks * stats.f_frsize) / divisor;

  auto free = pow_format(stats.f_bavail * stats.f_frsize, "B", true);
  auto used = pow_format((stats.f_blocks - stats.f_bfree) * stats.f_frsize, "B", true);
  auto total = pow_format(stats.f_blocks * stats.f_frsize, "B", true);
  auto percentage_used = (stats.f_blocks - stats.f_bfree) * 100 / stats.f_blocks;

  auto format = format_;
  auto state = getState(percentage_used);
  if (!state.empty() && config_["format-" + state].isString()) {
    format = config_["format-" + state].asString();
  }

  if (format.empty()) {
    event_box_.hide();
  } else {
    event_box_.show();
    label_.set_markup(fmt::format(
        fmt::runtime(format), stats.f_bavail * 100 / stats.f_blocks, fmt::arg("free", free),
        fmt::arg("percentage_free", stats.f_bavail * 100 / stats.f_blocks), fmt::arg("used", used),
        fmt::arg("percentage_used", percentage_used), fmt::arg("total", total),
        fmt::arg("path", path_), fmt::arg("specific_free", specific_free),
        fmt::arg("specific_used", specific_used), fmt::arg("specific_total", specific_total)));
  }

  if (tooltipEnabled()) {
    std::string tooltip_format = "{used} used out of {total} on {path} ({percentage_used}%)";
    if (config_["tooltip-format"].isString()) {
      tooltip_format = config_["tooltip-format"].asString();
    }
    label_.set_tooltip_text(fmt::format(
        fmt::runtime(tooltip_format), stats.f_bavail * 100 / stats.f_blocks, fmt::arg("free", free),
        fmt::arg("percentage_free", stats.f_bavail * 100 / stats.f_blocks), fmt::arg("used", used),
        fmt::arg("percentage_used", percentage_used), fmt::arg("total", total),
        fmt::arg("path", path_), fmt::arg("specific_free", specific_free),
        fmt::arg("specific_used", specific_used), fmt::arg("specific_total", specific_total)));
  }
  // Call parent update
  ALabel::update();
}

float waybar::modules::Disk::calc_specific_divisor(std::string divisor) {
  if (divisor == "kB") {
    return 1000.0;
  } else if (divisor == "kiB") {
    return 1024.0;
  } else if (divisor == "MB") {
    return 1000.0 * 1000.0;
  } else if (divisor == "MiB") {
    return 1024.0 * 1024.0;
  } else if (divisor == "GB") {
    return 1000.0 * 1000.0 * 1000.0;
  } else if (divisor == "GiB") {
    return 1024.0 * 1024.0 * 1024.0;
  } else if (divisor == "TB") {
    return 1000.0 * 1000.0 * 1000.0 * 1000.0;
  } else if (divisor == "TiB") {
    return 1024.0 * 1024.0 * 1024.0 * 1024.0;
  } else {  // default to Bytes if it is anything that we don't recongnise
    return 1.0;
  }
}