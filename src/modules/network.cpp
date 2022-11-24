#include "modules/network.hpp"

#include <linux/if.h>
#include <spdlog/spdlog.h>
#include <sys/eventfd.h>

#include <cassert>
#include <fstream>
#include <optional>
#include <sstream>

#include "util/format.hpp"
#ifdef WANT_RFKILL
#include "util/rfkill.hpp"
#endif

namespace {
using namespace waybar::util;
constexpr const char *DEFAULT_FORMAT = "{ifname}";
}  // namespace

constexpr const char *NETDEV_FILE =
    "/proc/net/dev";  // std::ifstream does not take std::string_view as param
std::optional<std::pair<unsigned long long, unsigned long long>>
waybar::modules::Network::readBandwidthUsage() {
  std::ifstream netdev(NETDEV_FILE);
  if (!netdev) {
    spdlog::warn("Failed to open netdev file {}", NETDEV_FILE);
    return {};
  }

  std::string line;
  // skip the headers (first two lines)
  std::getline(netdev, line);
  std::getline(netdev, line);

  unsigned long long receivedBytes = 0ull;
  unsigned long long transmittedBytes = 0ull;
  while (std::getline(netdev, line)) {
    std::istringstream iss(line);

    std::string ifacename;
    iss >> ifacename;      // ifacename contains "eth0:"
    ifacename.pop_back();  // remove trailing ':'
    if (ifacename != ifname_) {
      continue;
    }

    // The rest of the line consists of whitespace separated counts divided
    // into two groups (receive and transmit). Each group has the following
    // columns: bytes, packets, errs, drop, fifo, frame, compressed, multicast
    //
    // We only care about the bytes count, so we'll just ignore the 7 other
    // columns.
    unsigned long long r = 0ull;
    unsigned long long t = 0ull;
    // Read received bytes
    iss >> r;
    // Skip all the other columns in the received group
    for (int colsToSkip = 7; colsToSkip > 0; colsToSkip--) {
      // skip whitespace between columns
      while (iss.peek() == ' ') {
        iss.ignore();
      }
      // skip the irrelevant column
      while (iss.peek() != ' ') {
        iss.ignore();
      }
    }
    // Read transmit bytes
    iss >> t;

    receivedBytes += r;
    transmittedBytes += t;
  }

  return {{receivedBytes, transmittedBytes}};
}

waybar::modules::Network::Network(const std::string &id, const Json::Value &config)
    : ALabel(config, "network", id, DEFAULT_FORMAT, 60),
      ifid_(-1),
      family_(config["family"] == "ipv6" ? AF_INET6 : AF_INET),
      efd_(-1),
      ev_fd_(-1),
      want_route_dump_(false),
      want_link_dump_(false),
      want_addr_dump_(false),
      dump_in_progress_(false),
      cidr_(0),
      signal_strength_dbm_(0),
      signal_strength_(0),
#ifdef WANT_RFKILL
      rfkill_{RFKILL_TYPE_WLAN},
#endif
      frequency_(0.0) {

  // Start with some "text" in the module's label_. update() will then
  // update it. Since the text should be different, update() will be able
  // to show or hide the event_box_. This is to work around the case where
  // the module start with no text, but the the event_box_ is shown.
  label_.set_markup("<s></s>");

  auto bandwidth = readBandwidthUsage();
  if (bandwidth.has_value()) {
    bandwidth_down_total_ = (*bandwidth).first;
    bandwidth_up_total_ = (*bandwidth).second;
  } else {
    bandwidth_down_total_ = 0;
    bandwidth_up_total_ = 0;
  }

  if (!config_["interface"].isString()) {
    // "interface" isn't configured, then try to guess the external
    // interface currently used for internet.
    want_route_dump_ = true;
  } else {
    // Look for an interface that match "interface"
    // and then find the address associated with it.
    want_link_dump_ = true;
    want_addr_dump_ = true;
  }

  createEventSocket();
  createInfoSocket();

  dp.emit();
  // Ask for a dump of interfaces and then addresses to populate our
  // information. First the interface dump, and once done, the callback
  // will be called again which will ask for addresses dump.
  askForStateDump();
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
  nl_socket_modify_cb(ev_sock_, NL_CB_FINISH, NL_CB_CUSTOM, handleEventsDone, this);
  auto groups = RTMGRP_LINK | (family_ == AF_INET ? RTMGRP_IPV4_IFADDR : RTMGRP_IPV6_IFADDR);
  nl_join_groups(ev_sock_, groups);  // Deprecated
  if (nl_connect(ev_sock_, NETLINK_ROUTE) != 0) {
    throw std::runtime_error("Can't connect network socket");
  }
  if (nl_socket_set_nonblocking(ev_sock_)) {
    throw std::runtime_error("Can't set non-blocking on network socket");
  }
  nl_socket_add_membership(ev_sock_, RTNLGRP_LINK);
  if (family_ == AF_INET) {
    nl_socket_add_membership(ev_sock_, RTNLGRP_IPV4_IFADDR);
  } else {
    nl_socket_add_membership(ev_sock_, RTNLGRP_IPV6_IFADDR);
  }
  if (!config_["interface"].isString()) {
    if (family_ == AF_INET) {
      nl_socket_add_membership(ev_sock_, RTNLGRP_IPV4_ROUTE);
    } else {
      nl_socket_add_membership(ev_sock_, RTNLGRP_IPV6_ROUTE);
    }
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
    auto fd = nl_socket_get_fd(ev_sock_);
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
        if (events[i].data.fd == nl_socket_get_fd(ev_sock_)) {
          int rc = 0;
          // Read as many message as possible, until the socket blocks
          while (true) {
            errno = 0;
            rc = nl_recvmsgs_default(ev_sock_);
            if (rc == -NLE_AGAIN || errno == EAGAIN) {
              rc = 0;
              break;
            }
          }
          if (rc < 0) {
            spdlog::error("nl_recvmsgs_default error: {}", nl_geterror(-rc));
            thread_.stop();
            break;
          }
        } else {
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
    if (rfkill_.getState()) return "disabled";
#endif
    return "disconnected";
  }
  if (!carrier_) return "disconnected";
  if (ipaddr_.empty()) return "linked";
  if (essid_.empty()) return "ethernet";
  return "wifi";
}

auto waybar::modules::Network::update() -> void {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string tooltip_format;

  auto bandwidth = readBandwidthUsage();
  auto bandwidth_down = 0ull;
  auto bandwidth_up = 0ull;
  if (bandwidth.has_value()) {
    auto down_octets = (*bandwidth).first;
    auto up_octets = (*bandwidth).second;

    bandwidth_down = down_octets - bandwidth_down_total_;
    bandwidth_down_total_ = down_octets;

    bandwidth_up = up_octets - bandwidth_up_total_;
    bandwidth_up_total_ = up_octets;
  }

  if (!alt_) {
    auto state = getNetworkState();
    if (!state_.empty() && label_.get_style_context()->has_class(state_)) {
      label_.get_style_context()->remove_class(state_);
    }
    if (config_["format-" + state].isString()) {
      default_format_ = config_["format-" + state].asString();
    } else if (config_["format"].isString()) {
      default_format_ = config_["format"].asString();
    } else {
      default_format_ = DEFAULT_FORMAT;
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
      format_, fmt::arg("essid", essid_), fmt::arg("signaldBm", signal_strength_dbm_),
      fmt::arg("signalStrength", signal_strength_),
      fmt::arg("signalStrengthApp", signal_strength_app_), fmt::arg("ifname", ifname_),
      fmt::arg("netmask", netmask_), fmt::arg("ipaddr", ipaddr_), fmt::arg("gwaddr", gwaddr_),
      fmt::arg("cidr", cidr_), fmt::arg("frequency", fmt::format("{:.1f}", frequency_)),
      fmt::arg("icon", getIcon(signal_strength_, state_)),
      fmt::arg("bandwidthDownBits", pow_format(bandwidth_down * 8ull / interval_.count(), "b/s")),
      fmt::arg("bandwidthUpBits", pow_format(bandwidth_up * 8ull / interval_.count(), "b/s")),
      fmt::arg("bandwidthTotalBits",
               pow_format((bandwidth_up + bandwidth_down) * 8ull / interval_.count(), "b/s")),
      fmt::arg("bandwidthDownOctets", pow_format(bandwidth_down / interval_.count(), "o/s")),
      fmt::arg("bandwidthUpOctets", pow_format(bandwidth_up / interval_.count(), "o/s")),
      fmt::arg("bandwidthTotalOctets",
               pow_format((bandwidth_up + bandwidth_down) / interval_.count(), "o/s")),
      fmt::arg("bandwidthDownBytes", pow_format(bandwidth_down / interval_.count(), "B/s")),
      fmt::arg("bandwidthUpBytes", pow_format(bandwidth_up / interval_.count(), "B/s")),
      fmt::arg("bandwidthTotalBytes",
               pow_format((bandwidth_up + bandwidth_down) / interval_.count(), "B/s")));
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
          tooltip_format, fmt::arg("essid", essid_), fmt::arg("signaldBm", signal_strength_dbm_),
          fmt::arg("signalStrength", signal_strength_),
          fmt::arg("signalStrengthApp", signal_strength_app_), fmt::arg("ifname", ifname_),
          fmt::arg("netmask", netmask_), fmt::arg("ipaddr", ipaddr_), fmt::arg("gwaddr", gwaddr_),
          fmt::arg("cidr", cidr_), fmt::arg("frequency", fmt::format("{:.1f}", frequency_)),
          fmt::arg("icon", getIcon(signal_strength_, state_)),
          fmt::arg("bandwidthDownBits",
                   pow_format(bandwidth_down * 8ull / interval_.count(), "b/s")),
          fmt::arg("bandwidthUpBits", pow_format(bandwidth_up * 8ull / interval_.count(), "b/s")),
          fmt::arg("bandwidthTotalBits",
                   pow_format((bandwidth_up + bandwidth_down) * 8ull / interval_.count(), "b/s")),
          fmt::arg("bandwidthDownOctets", pow_format(bandwidth_down / interval_.count(), "o/s")),
          fmt::arg("bandwidthUpOctets", pow_format(bandwidth_up / interval_.count(), "o/s")),
          fmt::arg("bandwidthTotalOctets",
                   pow_format((bandwidth_up + bandwidth_down) / interval_.count(), "o/s")),
          fmt::arg("bandwidthDownBytes", pow_format(bandwidth_down / interval_.count(), "B/s")),
          fmt::arg("bandwidthUpBytes", pow_format(bandwidth_up / interval_.count(), "B/s")),
          fmt::arg("bandwidthTotalBytes",
                   pow_format((bandwidth_up + bandwidth_down) / interval_.count(), "B/s")));
      if (label_.get_tooltip_text() != tooltip_text) {
        label_.set_tooltip_markup(tooltip_text);
      }
    } else if (label_.get_tooltip_text() != text) {
      label_.set_tooltip_markup(text);
    }
  }

  // Call parent update
  ALabel::update();
}

bool waybar::modules::Network::checkInterface(std::string name) {
  if (config_["interface"].isString()) {
    return config_["interface"].asString() == name ||
           wildcardMatch(config_["interface"].asString(), name);
  }
  return false;
}

void waybar::modules::Network::clearIface() {
  ifid_ = -1;
  ifname_.clear();
  essid_.clear();
  ipaddr_.clear();
  gwaddr_.clear();
  netmask_.clear();
  carrier_ = false;
  cidr_ = 0;
  signal_strength_dbm_ = 0;
  signal_strength_ = 0;
  signal_strength_app_.clear();
  frequency_ = 0.0;
}

int waybar::modules::Network::handleEvents(struct nl_msg *msg, void *data) {
  auto net = static_cast<waybar::modules::Network *>(data);
  std::lock_guard<std::mutex> lock(net->mutex_);
  auto nh = nlmsg_hdr(msg);
  bool is_del_event = false;

  switch (nh->nlmsg_type) {
    case RTM_DELLINK:
      is_del_event = true;
    case RTM_NEWLINK: {
      struct ifinfomsg *ifi = static_cast<struct ifinfomsg *>(NLMSG_DATA(nh));
      ssize_t attrlen = IFLA_PAYLOAD(nh);
      struct rtattr *ifla = IFLA_RTA(ifi);
      const char *ifname = NULL;
      size_t ifname_len = 0;
      std::optional<bool> carrier;

      if (net->ifid_ != -1 && ifi->ifi_index != net->ifid_) {
        return NL_OK;
      }

      // Check if the interface goes "down" and if we want to detect the
      // external interface.
      if (net->ifid_ != -1 && !(ifi->ifi_flags & IFF_UP) && !net->config_["interface"].isString()) {
        // The current interface is now down, all the routes associated with
        // it have been deleted, so start looking for a new default route.
        spdlog::debug("network: if{} down", net->ifid_);
        net->clearIface();
        net->dp.emit();
        net->want_route_dump_ = true;
        net->askForStateDump();
        return NL_OK;
      }

      for (; RTA_OK(ifla, attrlen); ifla = RTA_NEXT(ifla, attrlen)) {
        switch (ifla->rta_type) {
          case IFLA_IFNAME:
            ifname = static_cast<const char *>(RTA_DATA(ifla));
            ifname_len = RTA_PAYLOAD(ifla) - 1;  // minus \0
            break;
          case IFLA_CARRIER: {
            carrier = *(char *)RTA_DATA(ifla) == 1;
            break;
          }
        }
      }

      if (!is_del_event && ifi->ifi_index == net->ifid_) {
        // Update interface information
        if (net->ifname_.empty() && ifname != NULL) {
          std::string new_ifname(ifname, ifname_len);
          net->ifname_ = new_ifname;
        }
        if (carrier.has_value()) {
          if (net->carrier_ != *carrier) {
            if (*carrier) {
              // Ask for WiFi information
              net->thread_timer_.wake_up();
            } else {
              // clear state related to WiFi connection
              net->essid_.clear();
              net->signal_strength_dbm_ = 0;
              net->signal_strength_ = 0;
              net->signal_strength_app_.clear();
              net->frequency_ = 0.0;
            }
          }
          net->carrier_ = carrier.value();
        }
      } else if (!is_del_event && net->ifid_ == -1) {
        // Checking if it's an interface we care about.
        std::string new_ifname(ifname, ifname_len);
        if (net->checkInterface(new_ifname)) {
          spdlog::debug("network: selecting new interface {}/{}", new_ifname, ifi->ifi_index);

          net->ifname_ = new_ifname;
          net->ifid_ = ifi->ifi_index;
          if (carrier.has_value()) {
            net->carrier_ = carrier.value();
          }
          net->thread_timer_.wake_up();
          /* An address for this new interface should be received via an
           * RTM_NEWADDR event either because we ask for a dump of both links
           * and addrs, or because this interface has just been created and
           * the addr will be sent after the RTM_NEWLINK event.
           * So we don't need to do anything. */
        }
      } else if (is_del_event && net->ifid_ >= 0) {
        // Our interface has been deleted, start looking/waiting for one we care.
        spdlog::debug("network: interface {}/{} deleted", net->ifname_, net->ifid_);

        net->clearIface();
        net->dp.emit();
      }
      break;
    }

    case RTM_DELADDR:
      is_del_event = true;
    case RTM_NEWADDR: {
      struct ifaddrmsg *ifa = static_cast<struct ifaddrmsg *>(NLMSG_DATA(nh));
      ssize_t attrlen = IFA_PAYLOAD(nh);
      struct rtattr *ifa_rta = IFA_RTA(ifa);

      if ((int)ifa->ifa_index != net->ifid_) {
        return NL_OK;
      }

      if (ifa->ifa_family != net->family_) {
        return NL_OK;
      }

      // We ignore address mark as scope for the link or host,
      // which should leave scope global addresses.
      if (ifa->ifa_scope >= RT_SCOPE_LINK) {
        return NL_OK;
      }

      for (; RTA_OK(ifa_rta, attrlen); ifa_rta = RTA_NEXT(ifa_rta, attrlen)) {
        switch (ifa_rta->rta_type) {
          case IFA_ADDRESS: {
            char ipaddr[INET6_ADDRSTRLEN];
            if (!is_del_event) {
              net->ipaddr_ = inet_ntop(ifa->ifa_family, RTA_DATA(ifa_rta), ipaddr, sizeof(ipaddr));
              net->cidr_ = ifa->ifa_prefixlen;
              switch (ifa->ifa_family) {
                case AF_INET: {
                  struct in_addr netmask;
                  netmask.s_addr = htonl(~0 << (32 - ifa->ifa_prefixlen));
                  net->netmask_ = inet_ntop(ifa->ifa_family, &netmask, ipaddr, sizeof(ipaddr));
                }
                case AF_INET6: {
                  struct in6_addr netmask;
                  for (int i = 0; i < 16; i++) {
                    int v = (i + 1) * 8 - ifa->ifa_prefixlen;
                    if (v < 0) v = 0;
                    if (v > 8) v = 8;
                    netmask.s6_addr[i] = ~0 << v;
                  }
                  net->netmask_ = inet_ntop(ifa->ifa_family, &netmask, ipaddr, sizeof(ipaddr));
                }
              }
              spdlog::debug("network: {}, new addr {}/{}", net->ifname_, net->ipaddr_, net->cidr_);
            } else {
              net->ipaddr_.clear();
              net->cidr_ = 0;
              net->netmask_.clear();
              spdlog::debug("network: {} addr deleted {}/{}", net->ifname_,
                            inet_ntop(ifa->ifa_family, RTA_DATA(ifa_rta), ipaddr, sizeof(ipaddr)),
                            ifa->ifa_prefixlen);
            }
            net->dp.emit();
            break;
          }
        }
      }
      break;
    }

      char temp_gw_addr[INET6_ADDRSTRLEN];
    case RTM_DELROUTE:
      is_del_event = true;
    case RTM_NEWROUTE: {
      // Based on https://gist.github.com/Yawning/c70d804d4b8ae78cc698
      // to find the interface used to reach the outside world

      struct rtmsg *rtm = static_cast<struct rtmsg *>(NLMSG_DATA(nh));
      ssize_t attrlen = RTM_PAYLOAD(nh);
      struct rtattr *attr = RTM_RTA(rtm);
      bool has_gateway = false;
      bool has_destination = false;
      int temp_idx = -1;
      uint32_t priority = 0;

      /* Find the message(s) concerting the main routing table, each message
       * corresponds to a single routing table entry.
       */
      if (rtm->rtm_table != RT_TABLE_MAIN) {
        return NL_OK;
      }

      /* Parse all the attributes for a single routing table entry. */
      for (; RTA_OK(attr, attrlen); attr = RTA_NEXT(attr, attrlen)) {
        /* Determine if this routing table entry corresponds to the default
         * route by seeing if it has a gateway, and if a destination addr is
         * set, that it is all 0s.
         */
        switch (attr->rta_type) {
          case RTA_GATEWAY:
            /* The gateway of the route.
             *
             * If someone ever needs to figure out the gateway address as well,
             * it's here as the attribute payload.
             */
            inet_ntop(net->family_, RTA_DATA(attr), temp_gw_addr, sizeof(temp_gw_addr));
            has_gateway = true;
            break;
          case RTA_DST: {
            /* The destination address.
             * Should be either missing, or maybe all 0s.  Accept both.
             */
            const uint32_t nr_zeroes = (net->family_ == AF_INET) ? 4 : 16;
            unsigned char c = 0;
            size_t dstlen = RTA_PAYLOAD(attr);
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
          case RTA_PRIORITY:
            priority = *(uint32_t *)RTA_DATA(attr);
            break;
          default:
            break;
        }
      }

      // Check if we have a default route.
      if (has_gateway && !has_destination && temp_idx != -1) {
        // Check if this is the first default route we see, or if this new
        // route have a higher priority.
        /** Module doesn`t update state, because RTA_GATEWAY call before enable new router and set
        higher priority. Disable router -> RTA_GATEWAY -> up new router -> set higher priority added
        checking route id
        **/
        if (!is_del_event &&
            ((net->ifid_ == -1) || (priority < net->route_priority) || (net->ifid_ != temp_idx))) {
          // Clear if's state for the case were there is a higher priority
          // route on a different interface.
          net->clearIface();
          net->ifid_ = temp_idx;
          net->route_priority = priority;
          net->gwaddr_ = temp_gw_addr;
          spdlog::debug("network: new default route via {} on if{} metric {}", temp_gw_addr,
                        temp_idx, priority);

          /* Ask ifname associated with temp_idx as well as carrier status */
          struct ifinfomsg ifinfo_hdr = {
              .ifi_family = AF_UNSPEC,
              .ifi_index = temp_idx,
          };
          int err;
          err = nl_send_simple(net->ev_sock_, RTM_GETLINK, NLM_F_REQUEST, &ifinfo_hdr,
                               sizeof(ifinfo_hdr));
          if (err < 0) {
            spdlog::error("network: failed to ask link info: {}", err);
            /* Ask for a dump of all links instead */
            net->want_link_dump_ = true;
          }

          /* Also ask for the address. Asking for a addresses of a specific
           * interface doesn't seems to work so ask for a dump of all
           * addresses. */
          net->want_addr_dump_ = true;
          net->askForStateDump();
          net->thread_timer_.wake_up();
        } else if (is_del_event && temp_idx == net->ifid_ && net->route_priority == priority) {
          spdlog::debug("network: default route deleted {}/if{} metric {}", net->ifname_, temp_idx,
                        priority);

          net->clearIface();
          net->dp.emit();
          /* Ask for a dump of all routes in case another one is already
           * setup. If there's none, there'll be an event with new one
           * later. */
          net->want_route_dump_ = true;
          net->askForStateDump();
        }
      }
      break;
    }
  }

  return NL_OK;
}

void waybar::modules::Network::askForStateDump(void) {
  /* We need to wait until the current dump is done before sending new
   * messages. handleEventsDone() is called when a dump is done. */
  if (dump_in_progress_) return;

  struct rtgenmsg rt_hdr = {
      .rtgen_family = AF_UNSPEC,
  };

  if (want_route_dump_) {
    rt_hdr.rtgen_family = family_;
    nl_send_simple(ev_sock_, RTM_GETROUTE, NLM_F_DUMP, &rt_hdr, sizeof(rt_hdr));
    want_route_dump_ = false;
    dump_in_progress_ = true;

  } else if (want_link_dump_) {
    nl_send_simple(ev_sock_, RTM_GETLINK, NLM_F_DUMP, &rt_hdr, sizeof(rt_hdr));
    want_link_dump_ = false;
    dump_in_progress_ = true;

  } else if (want_addr_dump_) {
    rt_hdr.rtgen_family = family_;
    nl_send_simple(ev_sock_, RTM_GETADDR, NLM_F_DUMP, &rt_hdr, sizeof(rt_hdr));
    want_addr_dump_ = false;
    dump_in_progress_ = true;
  }
}

int waybar::modules::Network::handleEventsDone(struct nl_msg *msg, void *data) {
  auto net = static_cast<waybar::modules::Network *>(data);
  net->dump_in_progress_ = false;
  net->askForStateDump();
  return NL_OK;
}

int waybar::modules::Network::handleScan(struct nl_msg *msg, void *data) {
  auto net = static_cast<waybar::modules::Network *>(data);
  auto gnlh = static_cast<genlmsghdr *>(nlmsg_data(nlmsg_hdr(msg)));
  struct nlattr *tb[NL80211_ATTR_MAX + 1];
  struct nlattr *bss[NL80211_BSS_MAX + 1];
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

  if (nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0),
                nullptr) < 0) {
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
    auto ies = static_cast<char *>(nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]));
    auto ies_len = nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
    const auto hdr_len = 2;
    while (ies_len > hdr_len && ies[0] != 0) {
      ies_len -= ies[1] + hdr_len;
      ies += ies[1] + hdr_len;
    }
    if (ies_len > hdr_len && ies_len > ies[1] + hdr_len) {
      auto essid_begin = ies + hdr_len;
      auto essid_end = essid_begin + ies[1];
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
    // WiFi-hardware usually operates in the range -90 to -30dBm.

    // If a signal is too strong, it can overwhelm receiving circuity that is designed
    // to pick up and process a certain signal level. The following percentage is scaled to
    // punish signals that are too strong (>= -45dBm) or too weak (<= -45 dBm).
    const int hardwareOptimum = -45;
    const int hardwareMin = -90;
    const int strength =
        100 -
        ((abs(signal_strength_dbm_ - hardwareOptimum) / double{hardwareOptimum - hardwareMin}) *
         100);
    signal_strength_ = std::clamp(strength, 0, 100);

    if (signal_strength_dbm_ >= -50) {
      signal_strength_app_ = "Great Connectivity";
    } else if (signal_strength_dbm_ >= -60) {
      signal_strength_app_ = "Good Connectivity";
    } else if (signal_strength_dbm_ >= -67) {
      signal_strength_app_ = "Streaming";
    } else if (signal_strength_dbm_ >= -70) {
      signal_strength_app_ = "Web Surfing";
    } else if (signal_strength_dbm_ >= -80) {
      signal_strength_app_ = "Basic Connectivity";
    } else {
      signal_strength_app_ = "Poor Connectivity";
    }
  }
  if (bss[NL80211_BSS_SIGNAL_UNSPEC] != nullptr) {
    signal_strength_ = nla_get_u8(bss[NL80211_BSS_SIGNAL_UNSPEC]);
  }
}

void waybar::modules::Network::parseFreq(struct nlattr **bss) {
  if (bss[NL80211_BSS_FREQUENCY] != nullptr) {
    // in GHz
    frequency_ = (double)nla_get_u32(bss[NL80211_BSS_FREQUENCY]) / 1000;
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
  if (genlmsg_put(nl_msg, NL_AUTO_PORT, NL_AUTO_SEQ, nl80211_id_, 0, NLM_F_DUMP,
                  NL80211_CMD_GET_SCAN, 0) == nullptr ||
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
