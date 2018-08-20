#pragma once

#include <pulse/pulseaudio.h>
#include <fmt/format.h>
#include <algorithm>
#include "ALabel.hpp"

namespace waybar::modules {

class Pulseaudio : public ALabel {
  public:
    Pulseaudio(const Json::Value&);
    ~Pulseaudio();
    auto update() -> void;
  private:
    static void subscribeCb(pa_context*, pa_subscription_event_type_t,
      uint32_t, void*);
    static void contextStateCb(pa_context*, void*);
    static void sinkInfoCb(pa_context*, const pa_sink_info*, int, void*);
    static void serverInfoCb(pa_context*, const pa_server_info*, void*);

    std::string getIcon(uint16_t);

    pa_threaded_mainloop* mainloop_;
    pa_mainloop_api* mainloop_api_;
    pa_context* context_;
    uint32_t sink_idx_{0};
    uint16_t volume_;
    bool muted_;
    std::string desc_;
};

}
