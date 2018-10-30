#pragma once

#include <fmt/format.h>
#include <pulse/pulseaudio.h>
#include <pulse/volume.h>
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
    static void volumeModifyCb(pa_context*, int, void*);
    bool handleScroll(GdkEventScroll* e);

    const std::string getPortIcon() const;

    pa_threaded_mainloop* mainloop_;
    pa_mainloop_api* mainloop_api_;
    pa_context* context_;
    uint32_t sink_idx_{0};
    uint16_t volume_;
    pa_cvolume pa_volume_;
    bool muted_;
    std::string port_name_;
    std::string desc_;
    bool scrolling_;
};

}  // namespace waybar::modules
