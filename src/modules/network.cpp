#include "modules/network.hpp"
#include <spdlog/spdlog.h>
#include <sys/eventfd.h>
#include <fstream>
#include <cassert>
#include <optional>
#include "util/format.hpp"
#ifdef WANT_RFKILL
#include "util/rfkill.hpp"
#endif

namespace {

using namespace waybar::util;

constexpr const char *NETSTAT_FILE =
    "/proc/net/netstat";  // std::ifstream does not take std::string_view as param
constexpr std::string_view BANDWIDTH_CATEGORY = "IpExt";
constexpr std::string_view BANDWIDTH_DOWN_TOTAL_KEY = "InOctets";
constexpr std::string_view BANDWIDTH_UP_TOTAL_KEY = "OutOctets";

std::ifstream                     netstat(NETSTAT_FILE);
std::optional<unsigned long long> read_netstat(std::string_view category, std::string_view key) {
  if (!netstat) {
    spdlog::warn("Failed to open netstat file {}", NETSTAT_FILE);
    return {};
  }
  netstat.seekg(std::ios_base::beg);

  // finding corresponding line (category)
  // looks into the file for the first line starting by the 'category' string
  auto starts_with = [](const std::string &str, std::string_view start) {
    return start == std::string_view{str.data(), std::min(str.size(), start.size())};
  };

  std::string read;
  while (std::getline(netstat, read) && !starts_with(read, category))
    ;
  if (!starts_with(read, category)) {
    spdlog::warn("Category '{}' not found in netstat file {}", category, NETSTAT_FILE);
    return {};
  }

  // finding corresponding column (key)
  // looks into the fetched line for the first word (space separated) equal to 'key'
  int  index = 0;
  auto r_it = read.begin();
  auto k_it = key.begin();
  while (k_it != key.end() && r_it != read.end()) {
    if (*r_it != *k_it) {
      r_it = std::find(r_it, read.end(), ' ');
      if (r_it != read.end()) {
        ++r_it;
      }
      k_it = key.begin();
      ++index;
    } else {
      ++r_it;
      ++k_it;
    }
  }

  if (r_it == read.end() && k_it != key.end()) {
    spdlog::warn(
        "Key '{}' not found in category '{}' of netstat file {}", key, category, NETSTAT_FILE);
    return {};
  }

  // finally accessing value
  // accesses the line right under the fetched one
  std::getline(netstat, read);
  assert(starts_with(read, category));
  std::istringstream iss(read);
  while (index--) {
    std::getline(iss, read, ' ');
  }
  unsigned long long value;
  iss >> value;
  return value;
}
}  // namespace

waybar::modules::Network::Network(const std::string &id, const Json::Value &config)
    : ALabel(config, "network", id, "{ifname}", 60),
      ifid_(-1),
      family_(config["family"] == "ipv6" ? AF_INET6 : AF_INET),
      efd_(-1),
      ev_fd_(-1),
      cidr_(-1),
      signal_strength_dbm_(0),
      signal_strength_(0),
#ifdef WANT_RFKILL
      rfkill_{RFKILL_TYPE_WLAN},
#endif
      frequency_(0) {
  auto down_octets = read_netstat(BANDWIDTH_CATEGORY, BANDWIDTH_DOWN_TOTAL_KEY);
  auto up_octets = read_netstat(BANDWIDTH_CATEGORY, BANDWIDTH_UP_TOTAL_KEY);
  if (down_octets) {
    bandwidth_down_total_ = *down_octets;
  } else {
    bandwidth_down_total_ = 0;
  }

  if (up_octets) {
    bandwidth_up_total_ = *up_octets;
  } else {
    bandwidth_up_total_ = 0;
  }

  createEventSocket();
  createInfoSocket();
  auto default_iface = getPreferredIface(-1, false);
  if (default_iface != -1) {
    ifid_ = default_iface;
    char ifname[IF_NAMESIZE];
    if_indextoname(default_iface, ifname);
    ifname_ = ifname;
    getInterfaceAddress();
  }
  dp.emit();
  worker();
}

waybar::modules::Network::~Network() {
  if (ev_fd_ > -1) {
    close(ev_fd_);
  }
  if (efd_ > -1) {
    close(efd_);
  }
  if (ev_sock_ != nullptr) {
    nl_socket_drop_membership(ev_sock_, RTNLGRP_LINK);
    if (family_ == AF_INET) {
      nl_socket_drop_membership(ev_sock_, RTNLGRP_IPV4_IFADDR);
    } else {
      nl_socket_drop_membership(ev_sock_, RTNLGRP_IPV6_IFADDR);
    }
    nl_close(ev_sock_);
    nl_socket_free(ev_sock_);
  }
  if (sock_ != nullptr) {
    nl_close(sock_);
    nl_socket_free(sock_);
  }
}

void waybar::modules::Network::createEventSocket() {
  ev_sock_ = nl_socket_alloc();
  nl_socket_disable_seq_check(ev_sock_);
  nl_socket_modify_cb(ev_sock_, NL_CB_VALID, NL_CB_CUSTOM, handleEvents, this);
  auto groups = RTMGRP_LINK | (family_ == AF_INET ? RTMGRP_IPV4_IFADDR : RTMGRP_IPV6_IFADDR);
  nl_join_groups(ev_sock_, groups);  // Deprecated
  if (nl_connect(ev_sock_, NETLINK_ROUTE) != 0) {
    throw std::runtime_error("Can't connect network socket");
  }
  nl_socket_add_membership(ev_sock_, RTNLGRP_LINK);
  if (family_ == AF_INET) {
    nl_socket_add_membership(ev_sock_, RTNLGRP_IPV4_IFADDR);
  } else {
    nl_socket_add_membership(ev_sock_, RTNLGRP_IPV6_IFADDR);
  }
  efd_ = epoll_create1(EPOLL_CLOEXEC);
  if (efd_ < 0) {
    throw std::runtime_error("Can't create epoll");
  }
  {
    ev_fd_ = eventfd(0, EFD_NONBLOCK);
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = ev_fd_;
    if (epoll_ctl(efd_, EPOLL_CTL_ADD, ev_fd_, &event) == -1) {
      throw std::runtime_error("Can't add epoll event");
    }
  }
  {
    auto               fd = nl_socket_get_fd(ev_sock_);
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    event.data.fd = fd;
    if (epoll_ctl(efd_, EPOLL_CTL_ADD, fd, &event) == -1) {
      throw std::runtime_error("Can't add epoll event");
    }
  }
}

void waybar::modules::Network::createInfoSocket() {
  sock_ = nl_socket_alloc();
  if (genl_connect(sock_) != 0) {
    throw std::runtime_error("Can't connect to netlink socket");
  }
  if (nl_socket_modify_cb(sock_, NL_CB_VALID, NL_CB_CUSTOM, handleScan, this) < 0) {
    throw std::runtime_error("Can't set callback");
  }
  nl80211_id_ = genl_ctrl_resolve(sock_, "nl80211");
  if (nl80211_id_ < 0) {
    spdlog::warn("Can't resolve nl80211 interface");
  }
}

void waybar::modules::Network::worker() {
  // update via here not working
  thread_timer_ = [this] {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (ifid_ > 0) {
        getInfo();
        dp.emit();
      }
    }
    thread_timer_.sleep_for(interval_);
  };
#ifdef WANT_RFKILL
  rfkill_.on_update.connect([this](auto &) {
    /* If we are here, it's likely that the network thread already holds the mutex and will be
     * holding it for a next few seconds.
     * Let's delegate the update to the timer thread instead of blocking the main thread.
     */
    thread_timer_.wake_up();
  });
#else
  spdlog::warn("Waybar has been built without rfkill support.");
#endif
  thread_ = [this] {
    std::array<struct epoll_event, EPOLL_MAX> events{};

    int ec = epoll_wait(efd_, events.data(), EPOLL_MAX, -1);
    if (ec > 0) {
      for (auto i = 0; i < ec; i++) {
        if (events[i].data.fd != nl_socket_get_fd(ev_sock_) || nl_recvmsgs_default(ev_sock_) < 0) {
          thread_.stop();
          break;
        }
      }
    }
  };
}

const std::string waybar::modules::Network::getNetworkState() const {
  if (ifid_ == -1) {
#ifdef WANT_RFKILL
    if (rfkill_.getState())
      return "disabled";
#endif
    return "disconnected";
  }
  if (ipaddr_.empty()) return "linked";
  if (essid_.empty()) return "ethernet";
  return "wifi";
}

auto waybar::modules::Network::update() -> void {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string                 tooltip_format;
  auto down_octets = read_netstat(BANDWIDTH_CATEGORY, BANDWIDTH_DOWN_TOTAL_KEY);
  auto up_octets = read_netstat(BANDWIDTH_CATEGORY, BANDWIDTH_UP_TOTAL_KEY);

  unsigned long long bandwidth_down = 0;
  if (down_octets) {
    bandwidth_down = *down_octets - bandwidth_down_total_;
    bandwidth_down_total_ = *down_octets;
  }

  unsigned long long bandwidth_up = 0;
  if (up_octets) {
    bandwidth_up = *up_octets - bandwidth_up_total_;
    bandwidth_up_total_ = *up_octets;
  }
  if (!alt_) {
    auto state = getNetworkState();
    if (!state_.empty() && label_.get_style_context()->has_class(state_)) {
      label_.get_style_context()->remove_class(state_);
    }
    if (config_["format-" + state].isString()) {
      default_format_ = config_["format-" + state].asString();
    }
    if (config_["tooltip-format-" + state].isString()) {
      tooltip_format = config_["tooltip-format-" + state].asString();
    }
    if (!label_.get_style_context()->has_class(state)) {
      label_.get_style_context()->add_class(state);
    }
    format_ = default_format_;
    state_ = state;
  }
  getState(signal_strength_);

  auto text = fmt::format(
      format_,
      fmt::arg("essid", essid_),
      fmt::arg("signaldBm", signal_strength_dbm_),
      fmt::arg("signalStrength", signal_strength_),
      fmt::arg("ifname", ifname_),
      fmt::arg("netmask", netmask_),
      fmt::arg("ipaddr", ipaddr_),
      fmt::arg("cidr", cidr_),
      fmt::arg("frequency", frequency_),
      fmt::arg("icon", getIcon(signal_strength_, state_)),
      fmt::arg("bandwidthDownBits", pow_format(bandwidth_down * 8ull / interval_.count(), "b/s")),
      fmt::arg("bandwidthUpBits", pow_format(bandwidth_up * 8ull / interval_.count(), "b/s")),
      fmt::arg("bandwidthDownOctets", pow_format(bandwidth_down / interval_.count(), "o/s")),
      fmt::arg("bandwidthUpOctets", pow_format(bandwidth_up / interval_.count(), "o/s")));
  if (text.compare(label_.get_label()) != 0) {
    label_.set_markup(text);
    if (text.empty()) {
      event_box_.hide();
    } else {
      event_box_.show();
    }
  }
  if (tooltipEnabled()) {
    if (tooltip_format.empty() && config_["tooltip-format"].isString()) {
      tooltip_format = config_["tooltip-format"].asString();
    }
    if (!tooltip_format.empty()) {
      auto tooltip_text = fmt::format(
          tooltip_format,
          fmt::arg("essid", essid_),
          fmt::arg("signaldBm", signal_strength_dbm_),
          fmt::arg("signalStrength", signal_strength_),
          fmt::arg("ifname", ifname_),
          fmt::arg("netmask", netmask_),
          fmt::arg("ipaddr", ipaddr_),
          fmt::arg("cidr", cidr_),
          fmt::arg("frequency", frequency_),
          fmt::arg("icon", getIcon(signal_strength_, state_)),
          fmt::arg("bandwidthDownBits",
                   pow_format(bandwidth_down * 8ull / interval_.count(), "b/s")),
          fmt::arg("bandwidthUpBits", pow_format(bandwidth_up * 8ull / interval_.count(), "b/s")),
          fmt::arg("bandwidthDownOctets", pow_format(bandwidth_down / interval_.count(), "o/s")),
          fmt::arg("bandwidthUpOctets", pow_format(bandwidth_up / interval_.count(), "o/s")));
      if (label_.get_tooltip_text() != tooltip_text) {
        label_.set_tooltip_text(tooltip_text);
      }
    } else if (label_.get_tooltip_text() != text) {
      label_.set_tooltip_text(text);
    }
  }

  // Call parent update
  ALabel::update();
}

// Based on https://gist.github.com/Yawning/c70d804d4b8ae78cc698
int waybar::modules::Network::getExternalInterface(int skip_idx) const {
  static const uint32_t route_buffer_size = 8192;
  struct nlmsghdr *     hdr = nullptr;
  struct rtmsg *        rt = nullptr;
  char                  resp[route_buffer_size] = {0};
  int                   ifidx = -1;

  /* Prepare request. */
  constexpr uint32_t reqlen = NLMSG_SPACE(sizeof(*rt));
  char               req[reqlen] = {0};

  /* Build the RTM_GETROUTE request. */
  hdr = reinterpret_cast<struct nlmsghdr *>(req);
  hdr->nlmsg_len = NLMSG_LENGTH(sizeof(*rt));
  hdr->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
  hdr->nlmsg_type = RTM_GETROUTE;
  rt = static_cast<struct rtmsg *>(NLMSG_DATA(hdr));
  rt->rtm_family = family_;
  rt->rtm_table = RT_TABLE_MAIN;

  /* Issue the query. */
  if (netlinkRequest(req, reqlen) < 0) {
    goto out;
  }

  /* Read the response(s).
   *
   * WARNING: All the packets generated by the request must be consumed (as in,
   * consume responses till NLMSG_DONE/NLMSG_ERROR is encountered).
   */
  do {
    auto len = netlinkResponse(resp, route_buffer_size);
    if (len < 0) {
      goto out;
    }

    /* Parse the response payload into netlink messages. */
    for (hdr = reinterpret_cast<struct nlmsghdr *>(resp); NLMSG_OK(hdr, len);
         hdr = NLMSG_NEXT(hdr, len)) {
      if (hdr->nlmsg_type == NLMSG_DONE) {
        goto out;
      }
      if (hdr->nlmsg_type == NLMSG_ERROR) {
        /* Even if we found the interface index, something is broken with the
         * netlink socket, so return an error.
         */
        ifidx = -1;
        goto out;
      }

      /* If we found the correct answer, skip parsing the attributes. */
      if (ifidx != -1) {
        continue;
      }

      /* Find the message(s) concerting the main routing table, each message
       * corresponds to a single routing table entry.
       */
      rt = static_cast<struct rtmsg *>(NLMSG_DATA(hdr));
      if (rt->rtm_table != RT_TABLE_MAIN) {
        continue;
      }

      /* Parse all the attributes for a single routing table entry. */
      struct rtattr *attr = RTM_RTA(rt);
      uint64_t       attrlen = RTM_PAYLOAD(hdr);
      bool           has_gateway = false;
      bool           has_destination = false;
      int            temp_idx = -1;
      for (; RTA_OK(attr, attrlen); attr = RTA_NEXT(attr, attrlen)) {
        /* Determine if this routing table entry corresponds to the default
         * route by seeing if it has a gateway, and if a destination addr is
         * set, that it is all 0s.
         */
        switch (attr->rta_type) {
          case RTA_GATEWAY:
            /* The gateway of the route.
             *
             * If someone every needs to figure out the gateway address as well,
             * it's here as the attribute payload.
             */
            has_gateway = true;
            break;
          case RTA_DST: {
            /* The destination address.
             * Should be either missing, or maybe all 0s.  Accept both.
             */
            const uint32_t nr_zeroes = (family_ == AF_INET) ? 4 : 16;
            unsigned char  c = 0;
            size_t         dstlen = RTA_PAYLOAD(attr);
            if (dstlen != nr_zeroes) {
              break;
            }
            for (uint32_t i = 0; i < dstlen; i += 1) {
              c |= *((unsigned char *)RTA_DATA(attr) + i);
            }
            has_destination = (c == 0);
            break;
          }
          case RTA_OIF:
            /* The output interface index. */
            temp_idx = *static_cast<int *>(RTA_DATA(attr));
            break;
          default:
            break;
        }
      }
      /* If this is the default route, and we know the interface index,
       * we can stop parsing this message.
       */
      if (has_gateway && !has_destination && temp_idx != -1 && temp_idx != skip_idx) {
        ifidx = temp_idx;
        break;
      }
    }
  } while (true);

out:
  return ifidx;
}

void waybar::modules::Network::getInterfaceAddress() {
  struct ifaddrs *ifaddr, *ifa;
  cidr_ = 0;
  int success = getifaddrs(&ifaddr);
  if (success != 0) {
    return;
  }
  ifa = ifaddr;
  while (ifa != nullptr) {
    if (ifa->ifa_addr != nullptr && ifa->ifa_addr->sa_family == family_ &&
        ifa->ifa_name == ifname_) {
      char ipaddr[INET6_ADDRSTRLEN];
      char netmask[INET6_ADDRSTRLEN];
      unsigned int cidr = 0;
      if (family_ == AF_INET) {
        ipaddr_ = inet_ntop(AF_INET,
                            &reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr)->sin_addr,
                            ipaddr,
                            INET_ADDRSTRLEN);
        auto net_addr = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_netmask);
        netmask_ = inet_ntop(AF_INET, &net_addr->sin_addr, netmask, INET_ADDRSTRLEN);
        unsigned int cidrRaw = net_addr->sin_addr.s_addr;
        while (cidrRaw) {
          cidr += cidrRaw & 1;
          cidrRaw >>= 1;
        }
      } else {
        ipaddr_ = inet_ntop(AF_INET6,
                            &reinterpret_cast<struct sockaddr_in6 *>(ifa->ifa_addr)->sin6_addr,
                            ipaddr,
                            INET6_ADDRSTRLEN);
        auto net_addr = reinterpret_cast<struct sockaddr_in6 *>(ifa->ifa_netmask);
        netmask_ = inet_ntop(AF_INET6, &net_addr->sin6_addr, netmask, INET6_ADDRSTRLEN);
        for (size_t i = 0; i < sizeof(net_addr->sin6_addr.s6_addr); ++i) {
          unsigned char cidrRaw = net_addr->sin6_addr.s6_addr[i];
          while (cidrRaw) {
            cidr += cidrRaw & 1;
            cidrRaw >>= 1;
          }
        }
      }
      cidr_ = cidr;
      break;
    }
    ifa = ifa->ifa_next;
  }
  freeifaddrs(ifaddr);
}

int waybar::modules::Network::netlinkRequest(void *req, uint32_t reqlen, uint32_t groups) const {
  struct sockaddr_nl sa = {};
  sa.nl_family = AF_NETLINK;
  sa.nl_groups = groups;
  struct iovec  iov = {req, reqlen};
  struct msghdr msg = {
      .msg_name = &sa,
      .msg_namelen = sizeof(sa),
      .msg_iov = &iov,
      .msg_iovlen = 1,
  };
  return sendmsg(nl_socket_get_fd(ev_sock_), &msg, 0);
}

int waybar::modules::Network::netlinkResponse(void *resp, uint32_t resplen, uint32_t groups) const {
  struct sockaddr_nl sa = {};
  sa.nl_family = AF_NETLINK;
  sa.nl_groups = groups;
  struct iovec  iov = {resp, resplen};
  struct msghdr msg = {
      .msg_name = &sa,
      .msg_namelen = sizeof(sa),
      .msg_iov = &iov,
      .msg_iovlen = 1,
  };
  auto ret = recvmsg(nl_socket_get_fd(ev_sock_), &msg, 0);
  if (msg.msg_flags & MSG_TRUNC) {
    return -1;
  }
  return ret;
}

bool waybar::modules::Network::checkInterface(struct ifinfomsg *rtif, std::string name) {
  if (config_["interface"].isString()) {
    return config_["interface"].asString() == name ||
           wildcardMatch(config_["interface"].asString(), name);
  }
  // getExternalInterface may need some delay to detect external interface
  for (uint8_t tries = 0; tries < MAX_RETRY; tries += 1) {
    auto external_iface = getExternalInterface();
    if (external_iface > 0) {
      return external_iface == rtif->ifi_index;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
  return false;
}

int waybar::modules::Network::getPreferredIface(int skip_idx, bool wait) const {
  int ifid = -1;
  if (config_["interface"].isString()) {
    ifid = if_nametoindex(config_["interface"].asCString());
    if (ifid > 0) {
      return ifid;
    } else {
      // Try with wildcard
      struct ifaddrs *ifaddr, *ifa;
      int             success = getifaddrs(&ifaddr);
      if (success != 0) {
        return -1;
      }
      ifa = ifaddr;
      ifid = -1;
      while (ifa != nullptr) {
        if (wildcardMatch(config_["interface"].asString(), ifa->ifa_name)) {
          ifid = if_nametoindex(ifa->ifa_name);
          break;
        }
        ifa = ifa->ifa_next;
      }
      freeifaddrs(ifaddr);
      return ifid;
    }
  }
  // getExternalInterface may need some delay to detect external interface
  for (uint8_t tries = 0; tries < MAX_RETRY; tries += 1) {
    ifid = getExternalInterface(skip_idx);
    if (ifid > 0) {
      return ifid;
    }
    if (wait) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  }
  return -1;
}

void waybar::modules::Network::clearIface() {
  essid_.clear();
  ipaddr_.clear();
  netmask_.clear();
  cidr_ = 0;
  signal_strength_dbm_ = 0;
  signal_strength_ = 0;
  frequency_ = 0;
}

void waybar::modules::Network::checkNewInterface(struct ifinfomsg *rtif) {
  auto new_iface = getPreferredIface(rtif->ifi_index);
  if (new_iface != -1) {
    ifid_ = new_iface;
    char ifname[IF_NAMESIZE];
    if_indextoname(new_iface, ifname);
    ifname_ = ifname;
    getInterfaceAddress();
    thread_timer_.wake_up();
  } else {
    ifid_ = -1;
    dp.emit();
  }
}

int waybar::modules::Network::handleEvents(struct nl_msg *msg, void *data) {
  auto                        net = static_cast<waybar::modules::Network *>(data);
  std::lock_guard<std::mutex> lock(net->mutex_);
  auto                        nh = nlmsg_hdr(msg);
  auto                        ifi = static_cast<struct ifinfomsg *>(NLMSG_DATA(nh));
  if (nh->nlmsg_type == RTM_DELADDR) {
    // Check for valid interface
    if (ifi->ifi_index == net->ifid_) {
      net->ipaddr_.clear();
      net->netmask_.clear();
      net->cidr_ = 0;
      if (!(ifi->ifi_flags & IFF_RUNNING)) {
        net->clearIface();
        // Check for a new interface and get info
        net->checkNewInterface(ifi);
      } else {
        net->dp.emit();
      }
      return NL_OK;
    }
  } else if (nh->nlmsg_type == RTM_NEWLINK || nh->nlmsg_type == RTM_DELLINK) {
    char ifname[IF_NAMESIZE];
    if_indextoname(ifi->ifi_index, ifname);
    // Check for valid interface
    if (ifi->ifi_index != net->ifid_ && net->checkInterface(ifi, ifname)) {
      net->ifname_ = ifname;
      net->ifid_ = ifi->ifi_index;
      // Get Iface and WIFI info
      net->getInterfaceAddress();
      net->thread_timer_.wake_up();
      return NL_OK;
    } else if (ifi->ifi_index == net->ifid_ &&
               (!(ifi->ifi_flags & IFF_RUNNING) || !(ifi->ifi_flags & IFF_UP) ||
                !net->checkInterface(ifi, ifname))) {
      net->clearIface();
      // Check for a new interface and get info
      net->checkNewInterface(ifi);
      return NL_OK;
    }
  } else {
    char ifname[IF_NAMESIZE];
    if_indextoname(ifi->ifi_index, ifname);
    // Auto detected network can also be assigned here
    if (ifi->ifi_index != net->ifid_ && net->checkInterface(ifi, ifname)) {
      // If iface is different, clear data
      if (ifi->ifi_index != net->ifid_) {
        net->clearIface();
      }
      net->ifname_ = ifname;
      net->ifid_ = ifi->ifi_index;
    }
    // Check for valid interface
    if (ifi->ifi_index == net->ifid_) {
      // Get Iface and WIFI info
      net->getInterfaceAddress();
      net->thread_timer_.wake_up();
      return NL_OK;
    }
  }
  return NL_SKIP;
}

int waybar::modules::Network::handleScan(struct nl_msg *msg, void *data) {
  auto              net = static_cast<waybar::modules::Network *>(data);
  auto              gnlh = static_cast<genlmsghdr *>(nlmsg_data(nlmsg_hdr(msg)));
  struct nlattr *   tb[NL80211_ATTR_MAX + 1];
  struct nlattr *   bss[NL80211_BSS_MAX + 1];
  struct nla_policy bss_policy[NL80211_BSS_MAX + 1]{};
  bss_policy[NL80211_BSS_TSF].type = NLA_U64;
  bss_policy[NL80211_BSS_FREQUENCY].type = NLA_U32;
  bss_policy[NL80211_BSS_BSSID].type = NLA_UNSPEC;
  bss_policy[NL80211_BSS_BEACON_INTERVAL].type = NLA_U16;
  bss_policy[NL80211_BSS_CAPABILITY].type = NLA_U16;
  bss_policy[NL80211_BSS_INFORMATION_ELEMENTS].type = NLA_UNSPEC;
  bss_policy[NL80211_BSS_SIGNAL_MBM].type = NLA_U32;
  bss_policy[NL80211_BSS_SIGNAL_UNSPEC].type = NLA_U8;
  bss_policy[NL80211_BSS_STATUS].type = NLA_U32;

  if (nla_parse(
          tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), nullptr) < 0) {
    return NL_SKIP;
  }
  if (tb[NL80211_ATTR_BSS] == nullptr) {
    return NL_SKIP;
  }
  if (nla_parse_nested(bss, NL80211_BSS_MAX, tb[NL80211_ATTR_BSS], bss_policy) != 0) {
    return NL_SKIP;
  }
  if (!net->associatedOrJoined(bss)) {
    return NL_SKIP;
  }
  net->parseEssid(bss);
  net->parseSignal(bss);
  net->parseFreq(bss);
  return NL_OK;
}

void waybar::modules::Network::parseEssid(struct nlattr **bss) {
  if (bss[NL80211_BSS_INFORMATION_ELEMENTS] != nullptr) {
    auto       ies = static_cast<char *>(nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]));
    auto       ies_len = nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
    const auto hdr_len = 2;
    while (ies_len > hdr_len && ies[0] != 0) {
      ies_len -= ies[1] + hdr_len;
      ies += ies[1] + hdr_len;
    }
    if (ies_len > hdr_len && ies_len > ies[1] + hdr_len) {
      auto        essid_begin = ies + hdr_len;
      auto        essid_end = essid_begin + ies[1];
      std::string essid_raw;
      std::copy(essid_begin, essid_end, std::back_inserter(essid_raw));
      essid_ = Glib::Markup::escape_text(essid_raw);
    }
  }
}

void waybar::modules::Network::parseSignal(struct nlattr **bss) {
  if (bss[NL80211_BSS_SIGNAL_MBM] != nullptr) {
    // signalstrength in dBm from mBm
    signal_strength_dbm_ = nla_get_s32(bss[NL80211_BSS_SIGNAL_MBM]) / 100;

    // WiFi-hardware usually operates in the range -90 to -20dBm.
    const int hardwareMax = -20;
    const int hardwareMin = -90;
    const int strength =
      ((signal_strength_dbm_ - hardwareMin) / double{hardwareMax - hardwareMin}) * 100;
    signal_strength_ = std::clamp(strength, 0, 100);
  }
  if (bss[NL80211_BSS_SIGNAL_UNSPEC] != nullptr) {
    signal_strength_ = nla_get_u8(bss[NL80211_BSS_SIGNAL_UNSPEC]);
  }
}

void waybar::modules::Network::parseFreq(struct nlattr **bss) {
  if (bss[NL80211_BSS_FREQUENCY] != nullptr) {
    // in MHz
    frequency_ = nla_get_u32(bss[NL80211_BSS_FREQUENCY]);
  }
}

bool waybar::modules::Network::associatedOrJoined(struct nlattr **bss) {
  if (bss[NL80211_BSS_STATUS] == nullptr) {
    return false;
  }
  auto status = nla_get_u32(bss[NL80211_BSS_STATUS]);
  switch (status) {
    case NL80211_BSS_STATUS_ASSOCIATED:
    case NL80211_BSS_STATUS_IBSS_JOINED:
    case NL80211_BSS_STATUS_AUTHENTICATED:
      return true;
    default:
      return false;
  }
}

auto waybar::modules::Network::getInfo() -> void {
  struct nl_msg *nl_msg = nlmsg_alloc();
  if (nl_msg == nullptr) {
    return;
  }
  if (genlmsg_put(
          nl_msg, NL_AUTO_PORT, NL_AUTO_SEQ, nl80211_id_, 0, NLM_F_DUMP, NL80211_CMD_GET_SCAN, 0) ==
          nullptr ||
      nla_put_u32(nl_msg, NL80211_ATTR_IFINDEX, ifid_) < 0) {
    nlmsg_free(nl_msg);
    return;
  }
  nl_send_sync(sock_, nl_msg);
}

// https://gist.github.com/rressi/92af77630faf055934c723ce93ae2495
bool waybar::modules::Network::wildcardMatch(const std::string &pattern,
                                             const std::string &text) const {
  auto P = int(pattern.size());
  auto T = int(text.size());

  auto p = 0, fallback_p = -1;
  auto t = 0, fallback_t = -1;

  while (t < T) {
    // Wildcard match:
    if (p < P && pattern[p] == '*') {
      fallback_p = p++;  // starting point after failures
      fallback_t = t;    // starting point after failures
    }

    // Simple match:
    else if (p < P && (pattern[p] == '?' || pattern[p] == text[t])) {
      p++;
      t++;
    }

    // Failure, fall back just after last matched '*':
    else if (fallback_p >= 0) {
      p = fallback_p + 1;  // position just after last matched '*"
      t = ++fallback_t;    // re-try to match text from here
    }

    // There were no '*' before, so we fail here:
    else {
      return false;
    }
  }

  // Consume all '*' at the end of pattern:
  while (p < P && pattern[p] == '*') p++;

  return p == P;
}
