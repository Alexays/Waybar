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
      Network(Json::Value config);
      auto update() -> void;
      operator Gtk::Widget &();
    private:
      void _parseEssid(struct nlattr **bss);
      void _parseSignal(struct nlattr **bss);
      bool _associatedOrJoined(struct nlattr **bss);
      static int _scanCb(struct nl_msg *msg, void *data);
      auto _getInfo() -> void;
      Gtk::Label _label;
      waybar::util::SleeperThread _thread;
      Json::Value _config;
      std::size_t _ifid;
      std::string _essid;
      int _signalStrengthdBm;
      int _signalStrength;
  };

}
