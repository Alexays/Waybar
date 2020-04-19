#include "modules/disk.hpp"

namespace waybar::modules {

Disk::Disk(const std::string& id, const Json::Value& config)
    : ALabel(config,
             "disk",
             id,
             "{}%",
             "{used} used out of {total} on {path} ({percentage_used}%)",
             30),
      path_("/") {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
  if (config["path"].isString()) {
    path_ = config["path"].asString();
  }
}

auto Disk::update(std::string format, waybar::args &args) -> void {
  struct statvfs stats;
  int err = statvfs(path_.c_str(), &stats);

  // Hide module on error
  if (err != 0) {
    event_box_.hide();
    return;
  }

  // Add default format and path
  auto percentageFree = stats.f_bavail * 100 / stats.f_blocks;
  args.push_back(percentageFree);
  args.push_back(fmt::arg("percentage_free", percentageFree));
  args.push_back(fmt::arg("path", path_));
  getState(percentageFree);

  if (ALabel::hasFormat("free")) {
    auto free = pow_format(stats.f_bavail * stats.f_bsize, "B", true);
    args.push_back(fmt::arg("free", free));
  }

  if (ALabel::hasFormat("used") || AModule::tooltipEnabled()) {
    auto used = pow_format((stats.f_blocks - stats.f_bavail) * stats.f_bsize, "B", true);
    args.push_back(fmt::arg("used", used));
  }

  if (ALabel::hasFormat("percentage_used") || AModule::tooltipEnabled()) {
    auto percentageUsed = (stats.f_blocks - stats.f_bavail) * 100 / stats.f_blocks);
    args.push_back(fmt::arg("percentage_used", percentageUsed));
  }

  if (ALabel::hasFormat("total") || AModule::tooltipEnabled()) {
    auto total = pow_format(stats.f_blocks * stats.f_bsize, "B", true);
    args.push_back(fmt::arg("total", total));
  }

  // Call parent update
  ALabel::update(format, args);
}

}  // namespace waybar::modules
