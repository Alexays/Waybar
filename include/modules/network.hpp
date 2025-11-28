#pragma once

#include <arpa/inet.h>
#include <fmt/format.h>
#include <linux/nl80211.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>
#include <netlink/netlink.h>
#include <sys/epoll.h>

#include <optional>
#include <vector>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"
#ifdef WANT_RFKILL
#include "util/rfkill.hpp"
#endif

enum ip_addr_pref : uint8_t { IPV4, IPV6, IPV4_6 };

namespace waybar::modules {

class Network : public ALabel {
 public:
  Network(const std::string&, const Json::Value&, std::mutex&, std::list<pid_t>&);
  virtual ~Network();
  auto update() -> void override;

 private:
  static const uint8_t MAX_RETRY{5};
  static const uint8_t EPOLL_MAX{200};

  static int handleEvents(struct nl_msg*, void*);
  static int handleEventsDone(struct nl_msg*, void*);
  static int handleScan(struct nl_msg*, void*);

  void askForStateDump(void);

  void worker();
  void createInfoSocket();
  void createEventSocket();
  void parseEssid(struct nlattr**);
  void parseSignal(struct nlattr**);
  void parseFreq(struct nlattr**);
  void parseBssid(struct nlattr**);
  bool associatedOrJoined(struct nlattr**);
  bool matchInterface(const std::string& ifname, const std::vector<std::string>& altnames,
                      std::string& matched) const;
  auto getInfo() -> void;
  const std::string getNetworkState() const;
  void clearIface();
  std::optional<std::pair<unsigned long long, unsigned long long>> readBandwidthUsage();

  int ifid_{-1};
  ip_addr_pref addr_pref_{ip_addr_pref::IPV4};
  struct sockaddr_nl nladdr_{0};
  struct nl_sock* sock_{nullptr};
  struct nl_sock* ev_sock_{nullptr};
  int efd_{-1};
  int ev_fd_{-1};
  int nl80211_id_{-1};
  std::mutex mutex_;

  bool want_route_dump_{false};
  bool want_link_dump_{false};
  bool want_addr_dump_{false};
  bool dump_in_progress_{false};
  bool is_p2p_{false};

  unsigned long long bandwidth_down_total_{0};
  unsigned long long bandwidth_up_total_{0};

  std::string state_;
  std::string essid_;
  std::string bssid_;
  bool carrier_{false};
  std::string ifname_;
  std::string ipaddr_;
  std::string ipaddr6_;
  std::string gwaddr_;
  std::string netmask_;
  std::string netmask6_;
  int cidr_{0};
  int cidr6_{0};
  int32_t signal_strength_dbm_;
  uint8_t signal_strength_;
  std::string signal_strength_app_;
  uint32_t route_priority;

  util::SleeperThread thread_;
  util::SleeperThread thread_timer_;
#ifdef WANT_RFKILL
  util::Rfkill rfkill_{RFKILL_TYPE_WLAN};
#endif
  float frequency_{0};
};

}  // namespace waybar::modules
