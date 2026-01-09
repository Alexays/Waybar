#include "modules/disk.hpp"

#include <spdlog/spdlog.h>

using namespace waybar::util;

waybar::modules::Disk::Disk(const std::string& id, const Json::Value& config)
    : ALabel(config, "disk", id, "{}%", 30), header_(""), paths_(), separator_(" ") {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
  if (config["header"].isString()) {
    header_ = config["header"].asString();
  }
  if (config["path"].isString() && !config["paths"].isArray()) {
    spdlog::warn("Disk: path is deprecated use paths instead!");
    paths_.push_back(config["path"].asString());
  }
  if (config["paths"].isArray()) {
    for (const auto& path : config["paths"]) {
      paths_.push_back(path.asString());
    }
  }
  if (!config["path"].isString() && !config["paths"].isArray()) {
    paths_.emplace_back("/");
  }
  if (config["separator"].isString()) {
    separator_ = config["separator"].asString();
  }
  if (config["unit"].isString()) {
    unit_ = config["unit"].asString();
  }
}

auto waybar::modules::Disk::update() -> void {
  std::string tooltip_label;
  std::string label = header_;

  bool had_valid_disk = false;

  for (size_t i = 0; i < paths_.size(); ++i) {
    const auto& path = paths_[i];

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

    int err = statvfs(path.c_str(), &stats);

    /* Conky options
      fs_bar - Bar that shows how much space is used
      fs_free - Free space on a file system
      fs_free_perc - Free percentage of space
      fs_size - File system size
      fs_used - File system used space
    */

    if (err != 0) {
      spdlog::warn("Disk: statvfs failed for path '{}' (errno={})", path, errno);
      continue;
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

    std::string disk_format = format_;
    auto state = getState(percentage_used);
    if (!state.empty() && config_["format-" + state].isString()) {
      disk_format = config_["format-" + state].asString();
    }

    if (!disk_format.empty()) {
      if (had_valid_disk) {
        label += separator_;
      }

      label += fmt::format(
          fmt::runtime(disk_format), stats.f_bavail * 100 / stats.f_blocks, fmt::arg("free", free),
          fmt::arg("percentage_free", stats.f_bavail * 100 / stats.f_blocks),
          fmt::arg("used", used), fmt::arg("percentage_used", percentage_used),
          fmt::arg("total", total), fmt::arg("path", path),
          fmt::arg("specific_free", specific_free), fmt::arg("specific_used", specific_used),
          fmt::arg("specific_total", specific_total));
    }

    std::string tooltip_format = "{used} used out of {total} on {path} ({percentage_used}%)";
    if (config_["tooltip-format"].isString()) {
      tooltip_format = config_["tooltip-format"].asString();
    }

    if (!tooltip_format.empty()) {
      if (had_valid_disk) {
        tooltip_label += "\n";
      }

      tooltip_label += fmt::format(
          fmt::runtime(tooltip_format), stats.f_bavail * 100 / stats.f_blocks,
          fmt::arg("free", free),
          fmt::arg("percentage_free", stats.f_bavail * 100 / stats.f_blocks),
          fmt::arg("used", used), fmt::arg("percentage_used", percentage_used),
          fmt::arg("total", total), fmt::arg("path", path),
          fmt::arg("specific_free", specific_free), fmt::arg("specific_used", specific_used),
          fmt::arg("specific_total", specific_total));
    }

    had_valid_disk = true;
  }
  if (had_valid_disk) {
    event_box_.show();
  } else {
    event_box_.hide();
  }

  label_.set_markup(label);

  if (tooltipEnabled() && !tooltip_label.empty()) {
    label_.set_tooltip_text(tooltip_label);
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
  } else {  // default to Bytes if it is anything that we don't recognise
    return 1.0;
  }
}
