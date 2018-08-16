#include "modules/network.hpp"

#include <iostream>

waybar::modules::Network::Network(Json::Value config)
  : _config(std::move(config)), _ifid(if_nametoindex(_config["interface"].asCString()))
{
  if (_ifid == 0) {
    throw std::runtime_error("Can't found network interface");
  }
  _label.set_name("network");
  int interval = _config["interval"] ? _config["inveral"].asInt() : 30;
  _thread = [this, interval] {
    Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Network::update));
    _thread.sleep_for(chrono::seconds(interval));
  };
};

auto waybar::modules::Network::update() -> void
{
  _getInfo();
  auto format = _config["format"] ? _config["format"].asString() : "{essid}";
  _label.set_text(fmt::format(format,
    fmt::arg("essid", _essid),
    fmt::arg("signaldBm", _signalStrengthdBm),
    fmt::arg("signalStrength", _signalStrength)
  ));
}

int waybar::modules::Network::_scanCb(struct nl_msg *msg, void *data) {
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
    if (!net->_associatedOrJoined(bss)) {
      return NL_SKIP;
    }
    net->_parseEssid(bss);
    net->_parseSignal(bss);
    // TODO(someone): parse quality
    return NL_SKIP;
}

void waybar::modules::Network::_parseEssid(struct nlattr **bss)
{
  _essid.clear();
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
      std::copy(essid_begin, essid_end, std::back_inserter(_essid));
    }
  }
}

void waybar::modules::Network::_parseSignal(struct nlattr **bss) {
    if (bss[NL80211_BSS_SIGNAL_MBM] != nullptr) {
      // signalstrength in dBm
      _signalStrengthdBm =
        static_cast<int>(nla_get_u32(bss[NL80211_BSS_SIGNAL_MBM])) / 100;

      // WiFi-hardware usually operates in the range -90 to -20dBm.
      const int hardwareMax = -20;
      const int hardwareMin = -90;
      _signalStrength = (static_cast<double>(_signalStrengthdBm - hardwareMin)
        / static_cast<double>(hardwareMax - hardwareMin)) * 100;
    }
  }

bool waybar::modules::Network::_associatedOrJoined(struct nlattr** bss)
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

auto waybar::modules::Network::_getInfo() -> void
{
	struct nl_sock *sk = nl_socket_alloc();
	if (genl_connect(sk) != 0) {
    nl_socket_free(sk);
    return;
  }
  if (nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM, _scanCb, this) < 0) {
    nl_socket_free(sk);
    return;
  }
  const int nl80211_id = genl_ctrl_resolve(sk, "nl80211");
  if (nl80211_id < 0) {
    nl_socket_free(sk);
    return;
  }
  struct nl_msg *msg = nlmsg_alloc();
  if (msg == nullptr) {
    nl_socket_free(sk);
    return;
  }
  if (genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, nl80211_id, 0, NLM_F_DUMP, NL80211_CMD_GET_SCAN, 0) == nullptr
    || nla_put_u32(msg, NL80211_ATTR_IFINDEX, _ifid) < 0) {
    nlmsg_free(msg);
    return;
  }
  nl_send_sync(sk, msg);
  nl_socket_free(sk);
}

waybar::modules::Network::operator Gtk::Widget &() {
  return _label;
}
