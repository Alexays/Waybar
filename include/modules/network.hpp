#pragma once

#include <net/if.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <linux/nl80211.h>
#include <json/json.h>
#include <fmt/format.h>
#include "util/chrono.hpp"
#include "IModule.hpp"

namespace waybar::modules {

class Network : public IModule {
  public:
    Network(Json::Value);
    auto update() -> void;
    operator Gtk::Widget &();
  private:
    static uint64_t netlinkRequest(int, void*, uint32_t, uint32_t groups = 0);
    static uint64_t netlinkResponse(int, void*, uint32_t, uint32_t groups = 0);
    static int scanCb(struct nl_msg*, void*);

    void disconnected();
    int getExternalInterface();
    void parseEssid(struct nlattr**);
    void parseSignal(struct nlattr**);
    bool associatedOrJoined(struct nlattr**);
    auto getInfo() -> void;

    Gtk::Label label_;
    waybar::util::SleeperThread thread_;
    Json::Value config_;

    int ifid_;
    sa_family_t family_;
    int sock_fd_;
    struct sockaddr_nl nladdr_ = {0};

    std::string essid_;
    std::string ifname_;
    int signal_strength_dbm_;
    uint16_t signal_strength_;
};

}
