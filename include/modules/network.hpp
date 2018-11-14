#pragma once

#include <net/if.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <linux/nl80211.h>
#include <fmt/format.h>
#include "util/chrono.hpp"
#include "ALabel.hpp"

namespace waybar::modules {

class Network : public ALabel {
  public:
    Network(const Json::Value&);
    ~Network();
    auto update() -> void;
  private:
    static int netlinkRequest(int, void*, uint32_t, uint32_t groups = 0);
    static int netlinkResponse(int, void*, uint32_t, uint32_t groups = 0);
    static int scanCb(struct nl_msg*, void*);

    void disconnected();
    void initNL80211();
    int getExternalInterface();
    void getInterfaceAddress();
    void parseEssid(struct nlattr**);
    void parseSignal(struct nlattr**);
    bool associatedOrJoined(struct nlattr**);
    auto getInfo() -> void;

    waybar::util::SleeperThread thread_;
    int ifid_;
    sa_family_t family_;
    int sock_fd_;
    struct sockaddr_nl nladdr_ = {};
    struct nl_sock* sk_ = nullptr;
    int nl80211_id_;

    std::string essid_;
    std::string ifname_;
    std::string ipaddr_;
    std::string netmask_;
    int cidr_;
    int signal_strength_dbm_;
    uint16_t signal_strength_;
};

}
