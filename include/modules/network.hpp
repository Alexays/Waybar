#pragma once

#include <net/if.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <linux/nl80211.h>
#include <iwlib.h> // TODO
#include <json/json.h>
#include <gtkmm.h>
#include <fmt/format.h>
#include <thread>
#include "util/chrono.hpp"
#include "IModule.hpp"

namespace waybar::modules {

  class Network : public IModule {
    public:
      Network(Json::Value config);
      auto update() -> void;
      operator Gtk::Widget &();
      typedef struct {
        int flags;
        char essid[IW_ESSID_MAX_SIZE + 1];
        uint8_t bssid[ETH_ALEN];
        int quality;
        int quality_max;
        int quality_average;
        int signal_level;
        int signal_level_max;
        int noise_level;
        int noise_level_max;
        int bitrate;
        double frequency;
      } wireless_info_t;
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
      int _signalStrength;
  };

}
