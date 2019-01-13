#pragma once

#include <net/if.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <linux/nl80211.h>
#include <sys/epoll.h>
#include <fmt/format.h>
#include "util/sleeper_thread.hpp"
#include "ALabel.hpp"

namespace waybar::modules {

class Network : public ALabel {
  public:
    Network(const std::string&, const Json::Value&);
    ~Network();
    auto update() -> void;
  private:
    static int handleEvents(struct nl_msg*, void*);
    static int handleScan(struct nl_msg*, void*);

    void worker();
    void disconnected();
    void createInfoSocket();
    void createEventSocket();
    int getExternalInterface();
    void getInterfaceAddress();
    int netlinkRequest(void*, uint32_t, uint32_t groups = 0);
    int netlinkResponse(void*, uint32_t, uint32_t groups = 0);
    void parseEssid(struct nlattr**);
    void parseSignal(struct nlattr**);
    bool associatedOrJoined(struct nlattr**);
    auto getInfo() -> void;

    waybar::util::SleeperThread thread_;
    waybar::util::SleeperThread thread_timer_;
    int ifid_;
    sa_family_t family_;
    struct sockaddr_nl nladdr_ = {0};
    struct nl_sock* sk_ = nullptr;
    struct nl_sock* info_sock_ = nullptr;
    int efd_;
    int ev_fd_;
    int nl80211_id_;

    std::string essid_;
    std::string ifname_;
    std::string ipaddr_;
    std::string netmask_;
    int cidr_;
    int32_t signal_strength_dbm_;
    uint8_t signal_strength_;
};

}
