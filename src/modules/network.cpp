#include <sys/eventfd.h>
#include "modules/network.hpp"

waybar::modules::Network::Network(const std::string& id, const Json::Value& config)
  : ALabel(config, "{ifname}", 60), family_(AF_INET), efd_(-1), ev_fd_(-1),
    cidr_(-1), signal_strength_dbm_(0), signal_strength_(0)
{
  label_.set_name("network");
  if (!id.empty()) {
    label_.get_style_context()->add_class(id);
  }
  createInfoSocket();
  createEventSocket();
  if (config_["interface"].isString()) {
    ifid_ = if_nametoindex(config_["interface"].asCString());
    ifname_ = config_["interface"].asString();
    if (ifid_ <= 0) {
      throw std::runtime_error("Can't found network interface");
    }
  } else {
    ifid_ = getExternalInterface();
    if (ifid_ > 0) {
      char ifname[IF_NAMESIZE];
      if_indextoname(ifid_, ifname);
      ifname_ = ifname;
    }
  }
  dp.emit();
  worker();
}

waybar::modules::Network::~Network()
{
  if (ev_fd_ > -1) {
    eventfd_write(ev_fd_, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    close(ev_fd_);
  }
  if (efd_ > -1) {
    close(efd_);
  }
  if (info_sock_ != nullptr) {
    nl_socket_drop_membership(info_sock_, RTMGRP_LINK);
    nl_socket_drop_membership(info_sock_, RTMGRP_IPV4_IFADDR);
    nl_close(info_sock_);
    nl_socket_free(info_sock_);
  }
  if (sk_ != nullptr) {
    nl_close(sk_);
    nl_socket_free(sk_);
  }
}

void waybar::modules::Network::createInfoSocket()
{
  info_sock_ = nl_socket_alloc();
  if (nl_connect(info_sock_, NETLINK_ROUTE) != 0) {
		throw std::runtime_error("Can't connect network socket");
	}
  if (nl_socket_add_membership(info_sock_, RTMGRP_LINK) != 0) {
    throw std::runtime_error("Can't add membership");
  }
  if (nl_socket_add_membership(info_sock_, RTMGRP_IPV4_IFADDR) != 0) {
    throw std::runtime_error("Can't add membership");
  }
  nl_socket_disable_seq_check(info_sock_);
  nl_socket_set_nonblocking(info_sock_);
  nl_socket_modify_cb(info_sock_, NL_CB_MSG_IN, NL_CB_CUSTOM, handleEvents, this);
  efd_ = epoll_create1(0);
  if (efd_ < 0) {
    throw std::runtime_error("Can't create epoll");
  }
  {
    ev_fd_ = eventfd(0, EFD_NONBLOCK);
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = ev_fd_;
    if (epoll_ctl(efd_, EPOLL_CTL_ADD, ev_fd_, &event) == -1) {
      throw std::runtime_error("Can't add epoll event");
    }
  }
  {
    auto fd = nl_socket_get_fd(info_sock_);
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = fd;
    if (epoll_ctl(efd_, EPOLL_CTL_ADD, fd, &event) == -1) {
      throw std::runtime_error("Can't add epoll event");
    }
  }
}

void waybar::modules::Network::createEventSocket()
{
  sk_ = nl_socket_alloc();
	if (genl_connect(sk_) != 0) {
    throw std::runtime_error("Can't connect to netlink socket");
  }
  if (nl_socket_modify_cb(sk_, NL_CB_VALID, NL_CB_CUSTOM, handleScan, this) < 0) {
    throw std::runtime_error("Can't set callback");
  }
  nl80211_id_ = genl_ctrl_resolve(sk_, "nl80211");
  if (nl80211_id_ < 0) {
    throw std::runtime_error("Can't resolve nl80211 interface");
  }
}

void waybar::modules::Network::worker()
{
  thread_timer_ = [this] {
    if (ifid_ > 0) {
      getInfo();
      dp.emit();
    }
    thread_timer_.sleep_for(interval_);
  };
  thread_ = [this] {
    struct epoll_event events[16];
    int ec = epoll_wait(efd_, events, 16, -1);
    if (ec > 0) {
      for (auto i = 0; i < ec; i++) {
        if (events[i].events & EPOLLIN
          && events[i].data.fd == nl_socket_get_fd(info_sock_)) {
          nl_recvmsgs_default(info_sock_);
        } else {
          thread_.stop();
          break;
        }
      }
    } else if (ec == -1) {
      thread_.stop();
    }
  };
}

auto waybar::modules::Network::update() -> void
{
  std::string connectiontype;
  if (ifid_ <= 0 || ipaddr_.empty()) {
    if (config_["format-disconnected"].isString()) {
      default_format_ = config_["format-disconnected"].asString();
    }
    label_.get_style_context()->add_class("disconnected");
    connectiontype = "disconnected";
  } else {
    if (essid_.empty()) {
      if (config_["format-ethernet"].isString()) {
        default_format_ = config_["format-ethernet"].asString();
      }
      connectiontype = "ethernet";
    } else {
      if (config_["format-wifi"].isString()) {
        default_format_ = config_["format-wifi"].asString();
      }
      connectiontype = "wifi";
    }
    label_.get_style_context()->remove_class("disconnected");
  }
  if (!alt_) {
    format_ = default_format_;
  }
  label_.set_markup(fmt::format(format_,
    fmt::arg("essid", essid_),
    fmt::arg("signaldBm", signal_strength_dbm_),
    fmt::arg("signalStrength", signal_strength_),
    fmt::arg("ifname", ifname_),
    fmt::arg("netmask", netmask_),
    fmt::arg("ipaddr", ipaddr_),
    fmt::arg("cidr", cidr_),
    fmt::arg("icon", getIcon(signal_strength_, connectiontype))
  ));
}

void waybar::modules::Network::disconnected()
{
  essid_.clear();
  signal_strength_dbm_ = 0;
  signal_strength_ = 0;
  ipaddr_.clear();
  netmask_.clear();
  cidr_ = 0;
  if (!config_["interface"].isString()) {
    ifname_.clear();
    ifid_ = -1;
  }
  // Need to wait otherwise we'll have the same information
  thread_.sleep_for(std::chrono::seconds(1));
}

// Based on https://gist.github.com/Yawning/c70d804d4b8ae78cc698
int waybar::modules::Network::getExternalInterface()
{
  static const uint32_t route_buffer_size = 8192;
  struct nlmsghdr *hdr = nullptr;
  struct rtmsg *rt = nullptr;
  char resp[route_buffer_size] = {0};
  int ifidx = -1;

  /* Prepare request. */
  constexpr uint32_t reqlen = NLMSG_SPACE(sizeof(*rt));
  char req[reqlen] = {0};

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
      uint64_t attrlen = RTM_PAYLOAD(hdr);
      bool has_gateway = false;
      bool has_destination = false;
      int temp_idx = -1;
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
            temp_idx = *static_cast<int*>(RTA_DATA(attr));
            break;
          default:
            break;
        }
      }
      /* If this is the default route, and we know the interface index,
       * we can stop parsing this message.
       */
      if (has_gateway && !has_destination && temp_idx != -1) {
        ifidx = temp_idx;
        break;
      }
    }
  } while (true);

out:
  return ifidx;
}

void waybar::modules::Network::getInterfaceAddress() {
  unsigned int cidrRaw;
  struct ifaddrs *ifaddr, *ifa;
  int success = getifaddrs(&ifaddr);
  if (success == 0) {
    ifa = ifaddr;
    while (ifa != nullptr && ipaddr_.empty() && netmask_.empty()) {
      if (ifa->ifa_addr != nullptr && ifa->ifa_addr->sa_family == family_) {
        if (strcmp(ifa->ifa_name, ifname_.c_str()) == 0) {
          ipaddr_ = inet_ntoa(((struct sockaddr_in*)ifa->ifa_addr)->sin_addr);
          netmask_ = inet_ntoa(((struct sockaddr_in*)ifa->ifa_netmask)->sin_addr);
          cidrRaw = ((struct sockaddr_in *)(ifa->ifa_netmask))->sin_addr.s_addr;
          unsigned int cidr = 0;
          while (cidrRaw) {
              cidr += cidrRaw & 1;
              cidrRaw >>= 1;
          }
          cidr_ = cidr;
        }
      }
      ifa = ifa->ifa_next;
    }
    freeifaddrs(ifaddr);
  } else {
    ipaddr_.clear();
    netmask_.clear();
    cidr_ = 0;
  }
}

int waybar::modules::Network::netlinkRequest(void *req,
  uint32_t reqlen, uint32_t groups)
{
  struct sockaddr_nl sa = {};
  sa.nl_family = AF_NETLINK;
  sa.nl_groups = groups;
  struct iovec iov = { req, reqlen };
  struct msghdr msg = { &sa, sizeof(sa), &iov, 1, nullptr, 0, 0 };
  return sendmsg(nl_socket_get_fd(info_sock_), &msg, 0);
}

int waybar::modules::Network::netlinkResponse(void *resp,
  uint32_t resplen, uint32_t groups)
{
  struct sockaddr_nl sa = {};
  sa.nl_family = AF_NETLINK;
  sa.nl_groups = groups;
  struct iovec iov = { resp, resplen };
  struct msghdr msg = { &sa, sizeof(sa), &iov, 1, nullptr, 0, 0 };
  auto ret = recvmsg(nl_socket_get_fd(info_sock_), &msg, 0);
  if (msg.msg_flags & MSG_TRUNC) {
    return -1;
  }
  return ret;
}

int waybar::modules::Network::handleEvents(struct nl_msg *msg, void *data) {
  auto net = static_cast<waybar::modules::Network *>(data);
  bool need_update = false;
  nlmsghdr *nh = nlmsg_hdr(msg);
  if (nh->nlmsg_type == RTM_NEWADDR) {
    need_update = true;
  }
  if (nh->nlmsg_type < RTM_NEWADDR) {
    auto rtif = static_cast<struct ifinfomsg *>(NLMSG_DATA(nh));
    if (rtif->ifi_index == static_cast<int>(net->ifid_)) {
      need_update = true;
      if (!(rtif->ifi_flags & IFF_RUNNING)) {
        net->disconnected();
      }
    }
  }
  if (net->ifid_ <= 0 && !net->config_["interface"].isString()) {
    // Need to wait before get external interface
    net->thread_.sleep_for(std::chrono::seconds(1));
    net->ifid_ = net->getExternalInterface();
    if (net->ifid_ > 0) {
      char ifname[IF_NAMESIZE];
      if_indextoname(net->ifid_, ifname);
      net->ifname_ = ifname;
      need_update = true;
    }
  }
  if (need_update) {
    if (net->ifid_ > 0) {
      net->getInfo();
    }
    net->dp.emit();
  }
  return NL_SKIP;
}

int waybar::modules::Network::handleScan(struct nl_msg *msg, void *data) {
  auto net = static_cast<waybar::modules::Network *>(data);
  auto gnlh = static_cast<genlmsghdr *>(nlmsg_data(nlmsg_hdr(msg)));
  struct nlattr* tb[NL80211_ATTR_MAX + 1];
  struct nlattr* bss[NL80211_BSS_MAX + 1];
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

  if (nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), nullptr) < 0) {
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
  // TODO(someone): parse quality
  return NL_SKIP;
}

void waybar::modules::Network::parseEssid(struct nlattr **bss)
{
  essid_.clear();
  if (bss[NL80211_BSS_INFORMATION_ELEMENTS] != nullptr) {
    auto ies =
      static_cast<char*>(nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]));
    auto ies_len = nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
    const auto hdr_len = 2;
    while (ies_len > hdr_len && ies[0] != 0) {
      ies_len -= ies[1] + hdr_len;
      ies += ies[1] + hdr_len;
    }
    if (ies_len > hdr_len && ies_len > ies[1] + hdr_len) {
      auto essid_begin = ies + hdr_len;
      auto essid_end = essid_begin + ies[1];
      std::copy(essid_begin, essid_end, std::back_inserter(essid_));
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
    signal_strength_ = ((signal_strength_dbm_ - hardwareMin)
      / double{hardwareMax - hardwareMin}) * 100;
  }
  if (bss[NL80211_BSS_SIGNAL_UNSPEC] != nullptr) {
    signal_strength_ = nla_get_u8(bss[NL80211_BSS_SIGNAL_UNSPEC]);
  }
}

bool waybar::modules::Network::associatedOrJoined(struct nlattr** bss)
{
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

auto waybar::modules::Network::getInfo() -> void
{
  getInterfaceAddress();
  struct nl_msg* nl_msg = nlmsg_alloc();
  if (nl_msg == nullptr) {
    return;
  }
  if (genlmsg_put(nl_msg, NL_AUTO_PORT, NL_AUTO_SEQ, nl80211_id_, 0, NLM_F_DUMP,
    NL80211_CMD_GET_SCAN, 0) == nullptr
    || nla_put_u32(nl_msg, NL80211_ATTR_IFINDEX, ifid_) < 0) {
    nlmsg_free(nl_msg);
    return;
  }
  nl_send_sync(sk_, nl_msg);
}
