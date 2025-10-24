#pragma once

#include <fmt/format.h>
#include <libvirt/libvirt.h>
#include <sys/statvfs.h>

#include <fstream>
#include <vector>

#include "ALabel.hpp"
#include "util/format.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

const std::string LIBVIRT_STATE_MAP[] = {"NOSTATE",  "RUNNING", "BLOCKED", "PAUSED",
                                         "SHUTDOWN", "SHUTOFF", "CRASHED", "PMSUSPENDED"};
class VMInfo {
 public:
  VMInfo(const std::string name, const virDomainInfo info) : name_(name), info_(info) {}

  std::string getName() const { return name_; }
  virDomainInfo getInfo() const { return info_; }

  bool operator==(const VMInfo &other) const {
    return name_ == other.name_ && info_.state == other.info_.state &&
           info_.maxMem == other.info_.maxMem && info_.memory == other.info_.memory &&
           info_.cpuTime == other.info_.cpuTime;
  }

  std::string string() const {
    return fmt::format("Name: {}, State: {}, MaxMem: {}, Memory: {}, CPUTime: {}", name_,
                       LIBVIRT_STATE_MAP[info_.state], info_.maxMem, info_.memory, info_.cpuTime);
  }

 private:
  std::string name_;
  virDomainInfo info_;
};

class Virtualization : public ALabel {
 public:
  Virtualization(const std::string &, const Json::Value &);
  virtual ~Virtualization() = default;
  auto update() -> void override;

 private:
  util::SleeperThread thread_;

  // URI according to RFC 2396 to libvirt to connect to.
  std::string virt_uri_;

  // Scaling factor for all eligible values of size.
  std::string unit_size_;
  int precision_size_;

  // Scaling factor for all eligible values of time.
  std::string unit_time_;
  int precision_time_;

  // Virtualization domains (typically VMs) to monitor.
  // If empty, all domains will be monitored.
  std::vector<std::string> domains_;

  // Fill the provided virtDomainInfo struct with the respective domain info.
  // If the domain does not exist, throw an error.
  void getDomainInfo(const std::string domain, virDomainInfoPtr info);

  // Get the domain info for all domains
  std::vector<VMInfo> getAllDomains();

  // For a list of domain infos, calculate the total cpu and memory and the max
  // they can consume.
  virDomainInfo getTotalDomainStats(std::vector<VMInfo> &VMInfos);

  // Convert a factor of time or size for scaling.
  float convertFactor(std::string divisor);

  // Round a float to a given precision.
  float roundToPrecision(float num, int precision);
};

}  // namespace waybar::modules