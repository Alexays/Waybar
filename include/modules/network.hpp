#pragma once

#include <arpa/inet.h>
#include <fmt/format.h>
#include <linux/nl80211.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>
#include <netlink/netlink.h>
#include <sys/epoll.h>

#include <optional>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"
#ifdef WANT_RFKILL
#include "util/rfkill.hpp"
#endif

namespace waybar::modules {

class Network : public ALabel {
 public:
  Network(const std::string&, const Json::Value&);
  virtual ~Network();
  auto update() -> void override;

 private:
  static const uint8_t MAX_RETRY = 5;
  static const uint8_t EPOLL_MAX = 200;

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
  bool associatedOrJoined(struct nlattr**);
  bool checkInterface(std::string name);
  auto getInfo() -> void;
  const std::string getNetworkState() const;
  void clearIface();
  bool wildcardMatch(const std::string& pattern, const std::string& text) const;
  std::optional<std::pair<unsigned long long, unsigned long long>> readBandwidthUsage();

  int ifid_;
  sa_family_t family_;
  struct sockaddr_nl nladdr_ = {0};
  struct nl_sock* sock_ = nullptr;
  struct nl_sock* ev_sock_ = nullptr;
  int efd_;
  int ev_fd_;
  int nl80211_id_;
  std::mutex mutex_;

  bool want_route_dump_;
  bool want_link_dump_;
  bool want_addr_dump_;
  bool dump_in_progress_;
  bool is_p2p_;

  unsigned long long bandwidth_down_total_;
  unsigned long long bandwidth_up_total_;

  std::string state_;
  std::string essid_;
  bool carrier_;
  std::string ifname_;
  std::string ipaddr_;
  std::string gwaddr_;
  std::string netmask_;
  int cidr_;
  int32_t signal_strength_dbm_;
  uint8_t signal_strength_;
  std::string signal_strength_app_;
  uint32_t route_priority;

  util::SleeperThread thread_;
  util::SleeperThread thread_timer_;
#ifdef WANT_RFKILL
  util::Rfkill rfkill_;
#endif
  float frequency_;
};

}  // namespace waybar::modules
