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
    static int scanCb(struct nl_msg*, void*);

    void parseEssid(struct nlattr**);
    void parseSignal(struct nlattr**);
    bool associatedOrJoined(struct nlattr**);
    auto getInfo() -> void;

    Gtk::Label label_;
    waybar::util::SleeperThread thread_;
    Json::Value config_;
    std::size_t ifid_;
    std::string essid_;
    int signal_strength_dbm_;
    uint16_t signal_strength_;
};

}
