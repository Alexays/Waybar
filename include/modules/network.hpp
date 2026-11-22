#pragma once

#include <arpa/inet.h>
#include <fmt/format.h>
#include <linux/nl80211.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>
#include <netlink/netlink.h>
#include <sigc++/trackable.h>
#include <sys/epoll.h>

#include <optional>
#include <vector>

#include "ALabel.hpp"
#include "giomm/dbusproxy.h"
#include "util/sleeper_thread.hpp"
#ifdef WANT_RFKILL
#include "util/rfkill.hpp"
#endif

#define ETH_ALEN 6

enum ip_addr_pref : uint8_t { IPV4, IPV6, IPV4_6 };

namespace waybar::modules {

// sigc::trackable, as in PowerProfilesDaemon: the NetworkManager proxies below
// are created with async callbacks bound via sigc::mem_fun, which must
// auto-disconnect if the module is destroyed while one is still in flight.
class Network : public ALabel, public sigc::trackable {
 public:
  Network(const std::string&, const Json::Value&);
  ~Network() override;
  auto update() -> void override;

 private:
  static const uint8_t EPOLL_MAX{200};

  static int handleEvents(struct nl_msg*, void*);
  static int handleEventsDone(struct nl_msg*, void*);
  static int handleDumpInterrupted(struct nl_msg*, void*);
  static int handleScan(struct nl_msg*, void*);
  static int handleStationGet(struct nl_msg* msg, void* data);

  void askForStateDump();

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
  bool isWireless() const;
  std::string getNetworkState() const;
  void clearIface();

  // NetworkManager is an optional enrichment layer over the netlink state
  // below: it is the only source for the "connecting" and "no-internet"
  // states, which cannot be derived from rtnetlink (see getNetworkState).
  // Everything here degrades to a no-op when NM is absent, unmanaged or
  // switched off, leaving the module purely netlink-driven as before.
  bool wantsNMState(const char* state) const;
  void nmInit();
  void nmBusConnectedCb(Glib::RefPtr<Gio::AsyncResult>& r);
  void nmManagerPropsChangedCb(const Gio::DBus::Proxy::MapChangedProperties& changed,
                               const std::vector<Glib::ustring>& invalidated);
  void nmReadManagerProps();
  // Requires mutex_ to be held by the caller.
  void nmBindDevice(const std::string& ifname);
  void nmDeviceResolvedCb(Glib::RefPtr<Gio::AsyncResult>& r);
  void nmDeviceProxyCb(Glib::RefPtr<Gio::AsyncResult>& r);
  void nmDevicePropsChangedCb(const Gio::DBus::Proxy::MapChangedProperties& changed,
                              const std::vector<Glib::ustring>& invalidated);
  void nmReadDeviceProps();
  std::optional<std::pair<unsigned long long, unsigned long long>> readBandwidthUsage();
  uint32_t readLinkSpeed() const;

  int ifid_{-1};
  ip_addr_pref addr_pref_{ip_addr_pref::IPV4};
  struct nl_sock* sock_{nullptr};
  struct nl_sock* ev_sock_{nullptr};
  struct nl_sock* station_sock_{nullptr};
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
  unsigned long long bandwidth_down_prev_{0};
  unsigned long long bandwidth_up_prev_{0};
  double bandwidth_sample_seconds_{1.0};
  std::chrono::steady_clock::time_point bandwidth_last_sample_time_;

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
  int32_t signal_strength_dbm_{0};
  uint8_t signal_strength_{0};
  std::string signal_strength_app_;
  uint32_t route_priority_;
  uint32_t link_speed_{0};

  util::SleeperThread thread_;
  util::SleeperThread thread_timer_;
#ifdef WANT_RFKILL
  util::Rfkill rfkill_{RFKILL_TYPE_WLAN};
#endif
  float frequency_{0};
  uint64_t tx_bitrate_{0};
  uint64_t rx_bitrate_{0};

  // Optional NetworkManager layer. want_nm_connecting_/want_nm_limited_ record
  // whether the user opted in by configuring the matching format, so that a
  // config without them keeps exactly the old five-state behavior and no DBus
  // connection is made at all.
  bool want_nm_connecting_{false};
  bool want_nm_limited_{false};
  Glib::RefPtr<Gio::DBus::Proxy> nm_proxy_;
  Glib::RefPtr<Gio::DBus::Proxy> nm_device_proxy_;
  // The interface nm_device_proxy_ is bound to, so update() can notice when
  // netlink picks a different one and rebind.
  std::string nm_bound_ifname_;
  uint32_t nm_device_state_{0};  // NM_DEVICE_STATE_UNKNOWN
  uint32_t nm_connectivity_{0};  // NM_CONNECTIVITY_UNKNOWN
  // NM's ConnectivityCheckEnabled. Without it the connectivity value above is
  // just "is there a default route", so it must not drive "no-internet".
  bool nm_connectivity_check_{false};

  // Minimum time "connecting" stays on screen once entered. A healthy
  // reconnect to a known AP activates in well under half a second, which the
  // eye reads as a glitch rather than as a state, so the state is held past the
  // end of the activation to make it legible. Zero disables the hold.
  std::chrono::milliseconds nm_connecting_min_{1000};
  std::chrono::steady_clock::time_point nm_connecting_since_{};
  // Re-renders the module when the hold above lapses; nothing else would, since
  // NM has already stopped sending us property changes by then.
  sigc::connection nm_connecting_hold_conn_;
};

}  // namespace waybar::modules
