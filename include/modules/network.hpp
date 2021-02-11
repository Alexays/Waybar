#pragma once

#include <arpa/inet.h>
#include <fmt/format.h>
#include <ifaddrs.h>
#include <linux/nl80211.h>
#include <net/if.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>
#include <netlink/netlink.h>
#include <sys/epoll.h>
#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"
#ifdef WANT_RFKILL
#include "util/rfkill.hpp"
#endif

namespace waybar::modules {

class Network : public ALabel {
 public:
  Network(const std::string&, const Json::Value&);
  ~Network();
  auto update() -> void;

 private:
  static const uint8_t MAX_RETRY = 5;
  static const uint8_t EPOLL_MAX = 200;

  static int handleEvents(struct nl_msg*, void*);
  static int handleScan(struct nl_msg*, void*);

  void              worker();
  void              createInfoSocket();
  void              createEventSocket();
  int               getExternalInterface(int skip_idx = -1) const;
  void              getInterfaceAddress();
  int               netlinkRequest(void*, uint32_t, uint32_t groups = 0) const;
  int               netlinkResponse(void*, uint32_t, uint32_t groups = 0) const;
  void              parseEssid(struct nlattr**);
  void              parseSignal(struct nlattr**);
  void              parseFreq(struct nlattr**);
  bool              associatedOrJoined(struct nlattr**);
  bool              checkInterface(struct ifinfomsg* rtif, std::string name);
  int               getPreferredIface(int skip_idx = -1, bool wait = true) const;
  auto              getInfo() -> void;
  void              checkNewInterface(struct ifinfomsg* rtif);
  const std::string getNetworkState() const;
  void              clearIface();
  bool              wildcardMatch(const std::string& pattern, const std::string& text) const;

  int                ifid_;
  sa_family_t        family_;
  struct sockaddr_nl nladdr_ = {0};
  struct nl_sock*    sock_ = nullptr;
  struct nl_sock*    ev_sock_ = nullptr;
  int                efd_;
  int                ev_fd_;
  int                nl80211_id_;
  std::mutex         mutex_;

  unsigned long long bandwidth_down_total_;
  unsigned long long bandwidth_up_total_;

  std::string state_;
  std::string essid_;
  std::string ifname_;
  std::string ipaddr_;
  std::string netmask_;
  int         cidr_;
  int32_t     signal_strength_dbm_;
  uint8_t     signal_strength_;
  uint32_t    frequency_;

  util::SleeperThread thread_;
  util::SleeperThread thread_timer_;
#ifdef WANT_RFKILL
  util::Rfkill rfkill_;
#endif
};

}  // namespace waybar::modules
