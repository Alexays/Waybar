#include "modules/virtualization.hpp"

#include <spdlog/spdlog.h>

#include <cmath>

using namespace waybar::util;
using waybar::modules::VMInfo;

waybar::modules::Virtualization::Virtualization(const std::string& id, const Json::Value& config)
    : ALabel(config, "libvirt", id, "{}%", 30) {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
  if (config["virt_uri"].isString()) {
    virt_uri_ = config["virt_uri"].asString();
  }

  unit_size_ = "G";
  if (config["unit_size"].isString()) {
    unit_size_ = config["unit_size"].asString();
  }

  unit_time_ = "s";
  if (config["unit_time"].isString()) {
    unit_time_ = config["unit_time"].asString();
  }

  precision_size_ = 2;
  if (config["precision_size"].isInt()) {
    precision_size_ = config["precision_size"].asInt();
  }

  precision_time_ = 2;
  if (config["precision_time"].isInt()) {
    precision_time_ = config["precision_time"].asInt();
  }

  const auto& configDomain = config["domains"];
  if (configDomain.isArray()) {
    domains_ = std::vector<std::string>{};
    for (const auto& i : configDomain) {
      domains_.push_back(i.asString());
    }
  }
}

void waybar::modules::Virtualization::getDomainInfo(const std::string name, virDomainInfoPtr info) {
  virConnectPtr con = nullptr;
  virDomainPtr dom = nullptr;
  int ret = EXIT_FAILURE;
  std::string err = "";

  con = virConnectOpenReadOnly(virt_uri_.c_str());
  if (con == nullptr) {
    err = "Unable to connect to hypervisor: " + virt_uri_;
    goto error;
  }

  /* Find the domain of the given name */
  dom = virDomainLookupByName(con, name.c_str());
  if (dom == nullptr) {
    err = "Failed to find Domain " + name;
    goto error;
  }

  /* Get the information */
  ret = virDomainGetInfo(dom, info);
  if (ret < 0) {
    err = "Failed to get information for Domain " + name;
    goto error;
  }

error:
  if (dom != nullptr) virDomainFree(dom);
  if (con != nullptr) virConnectClose(con);

  throw std::runtime_error(err);
}

std::vector<VMInfo> waybar::modules::Virtualization::getAllDomains() {
  virConnectPtr con = nullptr;
  virDomainPtr domain;
  std::vector<VMInfo> VMInfos = {};
  int numActiveDomains = 0;
  int numInactiveDomains = 0;

  con = virConnectOpenReadOnly(virt_uri_.c_str());
  if (con == nullptr) {
    throw std::runtime_error("Unable to connect to hypervisor: " + virt_uri_);
  }

  numActiveDomains = virConnectNumOfDomains(con);
  numInactiveDomains = virConnectNumOfDefinedDomains(con);
  int numDomains = numActiveDomains + numInactiveDomains;

  if (numDomains == 0) {
    throw std::runtime_error("No domains found");
  }

  char** inactiveDomains = new char*[numInactiveDomains];
  int activeDomains[numDomains] = {0};

  numActiveDomains = virConnectListDomains(con, activeDomains, numActiveDomains);
  numInactiveDomains = virConnectListDefinedDomains(con, inactiveDomains, numInactiveDomains);

  // active domains
  virDomainInfo info;
  for (int i = 0; i < numActiveDomains; i++) {
    domain = virDomainLookupByID(con, activeDomains[i]);
    const std::string name = std::string(virDomainGetName(domain));
    virDomainGetInfo(domain, &info);
    VMInfos.push_back(VMInfo(name, info));
  }

  // inactive domains
  for (int i = 0; i < numInactiveDomains; i++) {
    domain = virDomainLookupByName(con, inactiveDomains[i]);
    const std::string name = std::string(virDomainGetName(domain));
    virDomainGetInfo(domain, &info);
    VMInfos.push_back(VMInfo(name, info));
  }

  return VMInfos;
}

virDomainInfo waybar::modules::Virtualization::getTotalDomainStats(std::vector<VMInfo>& VMInfos) {
  virDomainInfo totalInfo = {0};

  for (const auto& info : VMInfos) {
    if (info.getInfo().state == VIR_DOMAIN_RUNNING) {
      totalInfo.memory += info.getInfo().memory;
      totalInfo.maxMem += info.getInfo().maxMem;
      totalInfo.nrVirtCpu += info.getInfo().nrVirtCpu;
      totalInfo.cpuTime += info.getInfo().cpuTime;
    }
  }
  return totalInfo;
}

auto waybar::modules::Virtualization::update() -> void {
  std::vector<waybar::modules::VMInfo> domains = getAllDomains();

  // If there are no user specified domains, we will show all domains
  if (domains_.empty()) {
    spdlog::info("libvirt: no domains supplied, showing all");
    // Empty domains means all
    domains_ = std::vector<std::string>{};
    for (const auto& domain : domains) {
      spdlog::info(fmt::format("libvirt: found domain {}", domain.string()));
    }

  } else {
    spdlog::info("libvirt: using specified domains");
    for (const auto& d : domains) {
      if (std::find(domains_.begin(), domains_.end(), d.getName()) == domains_.end()) {
        domains.erase(std::find(domains.begin(), domains.end(), d));
      }
    }
  }

  auto format = format_;
  if (format_.empty()) {
    event_box_.hide();
  } else {
    event_box_.show();

    const auto stats = getTotalDomainStats(domains);

    std::array<unsigned int, 8> DOMAIN_MAP;
    for (const auto& domain : domains) {
      DOMAIN_MAP[domain.getInfo().state]++;
    }

    const auto mem_max =
        roundToPrecision((stats.maxMem * 1000) / convertFactor(unit_size_), precision_size_);

    const auto mem_cur =
        roundToPrecision((stats.memory * 1000) / convertFactor(unit_size_), precision_size_);

    const auto cpu_time = (stats.cpuTime * 1000000) / convertFactor(unit_time_);

    if (tooltipEnabled()) {
      std::string tooltip_format = "{mem_cur} used out of {mem_max} ({domains})\nAll Domains:\n";
      for (const auto& v : domains) {
        tooltip_format += fmt::format("  - {}\n", v.string());
      }
      if (config_["tooltip-format"].isString()) {
        tooltip_format = config_["tooltip-format"].asString();
      }

      label_.set_tooltip_text(
          fmt::format(fmt::runtime(tooltip_format), fmt::arg("mem_max", mem_max),
                      fmt::arg("mem_cur", mem_cur), fmt::arg("domains", domains.size())));
    }

    label_.set_markup(fmt::format(
        fmt::runtime(format), fmt::arg("mem_max", mem_max), fmt::arg("mem_cur", mem_cur),
        fmt::arg("cpu_time", cpu_time), fmt::arg("domains_running", DOMAIN_MAP[1]),
        fmt::arg("domains_blocked", DOMAIN_MAP[2]), fmt::arg("domains_paused", DOMAIN_MAP[3]),
        fmt::arg("domains_shutdown", DOMAIN_MAP[4]), fmt::arg("domains_shutoff", DOMAIN_MAP[5]),
        fmt::arg("domains_crashed", DOMAIN_MAP[6]), fmt::arg("domains_suspended", DOMAIN_MAP[7]),
        fmt::arg("domains", domains.size())));
  }

  ALabel::update();
}

// Return the calculation factor to adjust the value from a factor to another.
float waybar::modules::Virtualization::convertFactor(std::string divisor) {
  if (divisor == "k" || divisor == "kB") {
    return 1000.0;
  } else if (divisor == "kiB") {
    return float(1 >> 10);
  } else if (divisor == "M" || divisor == "MB") {
    return float(1 << 20);
  } else if (divisor == "G" || divisor == "GB") {
    return 1000.0 * 1000.0 * 1000.0;
  } else if (divisor == "GiB") {
    return float(1 << 30);
  } else if (divisor == "T" || divisor == "TB") {
    return 1000.0 * 1000.0 * 1000.0 * 1000.0;
  } else if (divisor == "TiB") {
    return float(1 << 40);
  } else if (divisor == "us" || divisor == "ms") {
    return 0.0001;
  } else if (divisor == "ns") {
    return 0.0000001;
  } else if (divisor == "m") {
    return 60.0;
  } else if (divisor == "h") {
    return 60.0 * 60.0;
  } else if (divisor == "d") {
    return 60.0 * 60.0 * 24.0;
  } else if (divisor == "s" || divisor == "b") {
    return 1.0;
  } else {
    throw std::runtime_error("invalid scaling unit: " + divisor);
  }
}

float waybar::modules::Virtualization::roundToPrecision(float num, int precision) {
  float factor = pow(10, precision);
  return std::round(num * factor) / factor;
}
