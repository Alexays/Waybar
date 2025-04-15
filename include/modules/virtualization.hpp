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

class Virtualization : public ALabel {
 public:
  Virtualization(const std::string &, const Json::Value &);
  virtual ~Virtualization() = default;
  auto update() -> void override;

 private:
  util::SleeperThread thread_;

  // URI according to RFC 2396 to libvirt to connect to
  std::string virt_uri_;

  // Scaling factor for all eligible values of size
  std::string unit_size_;
  int precision_size_;

  // Scaling factor for all eligible values of time
  std::string unit_time_;
  int precision_time_;

  // Virtualization domains (typically VMs) to monitor
  // If empty, all domains will be monitored.
  std::vector<std::string> domains_;

  // Fill the provided virtDomainInfo struct with the respective domain info.
  // If the domain does not exist, throw an error.
  void getDomainInfo(const std::string domain, virDomainInfoPtr info);

  // For a list of domain infos, calculate the total cpu and memory and the max
  // they can consume.
  virDomainInfo getTotalDomainStats(std::vector<virDomainInfo> &domainInfos);

  // Convert a factor of time or size for scaling
  float convertFactor(std::string divisor);
};

}  // namespace waybar::modules