#include "modules/virtualization.hpp"

using namespace waybar::util;

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

  precision_size_ = 100;
  if (config["precision_size"].isInt()) {
    precision_size_ = config["precision_size"].asInt();
  }

  precision_time_ = 100;
  if (config["precision_time"].isInt()) {
    precision_time_ = config["precision_time"].asInt();
  }

  auto configDomain = config["domains"];
  if (configDomain.isArray()) {
    domains_ = std::vector<std::string>{};
    for (unsigned int i = 0; i < configDomain.size(); i++) {
      domains_.push_back(configDomain[i].asString());
    }
  }
}

void waybar::modules::Virtualization::getDomainInfo(const std::string name, virDomainInfoPtr info) {
  virConnectPtr con = NULL;
  virDomainPtr dom = NULL;
  int ret = EXIT_FAILURE;

  con = virConnectOpenReadOnly(virt_uri_.c_str());
  if (con == NULL) {
    throw std::runtime_error("Unable to connect to hypervisor: " + virt_uri_);
    goto error;
  }

  /* Find the domain of the given name */
  dom = virDomainLookupByName(con, name.c_str());
  if (dom == NULL) {
    throw std::runtime_error("Failed to find Domain " + name);
    goto error;
  }

  /* Get the information */
  ret = virDomainGetInfo(dom, info);
  if (ret < 0) {
    throw std::runtime_error("Failed to get information for Domain " + name);
    goto error;
  }

error:
  if (dom != NULL) virDomainFree(dom);
  if (con != NULL) virConnectClose(con);
}

virDomainInfo waybar::modules::Virtualization::getTotalDomainStats(
    std::vector<virDomainInfo>& domainInfos) {
  virDomainInfo totalInfo = {0};

  for (const auto& info : domainInfos) {
    totalInfo.memory += info.memory;
    totalInfo.maxMem += info.maxMem;
    totalInfo.nrVirtCpu += info.nrVirtCpu;
    totalInfo.cpuTime += info.cpuTime;
  }

  return totalInfo;
}

auto waybar::modules::Virtualization::update() -> void {
  virConnectPtr con = NULL;
  virDomainPtr dom = NULL;
  std::vector<virDomainInfo> domainInfos = {};

  con = virConnectOpenReadOnly(virt_uri_.c_str());
  if (con == NULL) {
    throw std::runtime_error("Unable to connect to hypervisor: " + virt_uri_);
    // goto error;
  }

  if (domains_.empty()) {
    // Empty domains means all
    const int n = virConnectNumOfDomains(con);
    int domains[n] = {0};
    virConnectListDomains(con, domains, n);

    auto activeDomains = malloc(sizeof(virDomainPtr) * n);

    virDomainInfo info;
    for (int i = 0; i < n; i++) {
      const auto vir = virDomainLookupByID(con, domains[i]);
      // TODO: check errors
      virDomainGetInfo(vir, &info);
      domainInfos.push_back(info);
    }

  } else {
    virDomainInfo info;
    for (const auto& domain : domains_) {
      getDomainInfo(domain, &info);
      domainInfos.push_back(info);
    }
  }

  const virDomainInfo stats = getTotalDomainStats(domainInfos);
  auto format = format_;

  if (format_.empty()) {
    event_box_.hide();
  } else {
    event_box_.show();
    // if (tooltipEnabled()) {
    //   std::string tooltip_format = "{mem_cur} used out of {mem_max}({domains})\n";
    //   if (config_["tooltip-format"].isString()) {
    //     tooltip_format = config_["tooltip-format"].asString();
    //   }

    //   label_.set_tooltip_text(
    //       fmt::format(fmt::runtime(tooltip_format),
    //                   fmt::arg("mem_max", (stats.maxMem / 1000) * convertFactor(unit_size_)),
    //                   fmt::arg("mem_cur", (stats.memory / 1000) * convertFactor(unit_size_)),
    //                   fmt::arg("cpu_time", (stats.cpuTime * 1000000) *
    //                   convertFactor(unit_time_)), fmt::arg("domains", domainInfos.size())));
    // } else {
    label_.set_markup(
        fmt::format(fmt::runtime(format),
                    fmt::arg("mem_max", (stats.maxMem / 1000) / convertFactor(unit_size_)),
                    fmt::arg("mem_cur", (stats.memory / 1000) / convertFactor(unit_size_)),
                    fmt::arg("cpu_time", (stats.cpuTime * 1000000) / convertFactor(unit_time_)),
                    fmt::arg("domains", domainInfos.size())));
    // }
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
