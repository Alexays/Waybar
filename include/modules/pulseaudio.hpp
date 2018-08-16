#pragma once

#include <pulse/pulseaudio.h>
#include <json/json.h>
#include <fmt/format.h>
#include <algorithm>
#include "IModule.hpp"

namespace waybar::modules {

class Pulseaudio : public IModule {
  public:
    Pulseaudio(Json::Value config);
    auto update() -> void;
    operator Gtk::Widget &();
  private:
    static void subscribeCb(pa_context*, pa_subscription_event_type_t,
      uint32_t, void*);
    static void contextStateCb(pa_context*, void*);
    static void sinkInfoCb(pa_context*, const pa_sink_info*, int, void*);
    static void serverInfoCb(pa_context*, const pa_server_info*, void*);

    std::string getIcon(uint16_t);

    Gtk::Label label_;
    Json::Value config_;
    pa_threaded_mainloop* mainloop_;
    pa_mainloop_api* mainloop_api_;
    pa_context* context_;
    uint32_t sink_idx_{0};
    uint16_t volume_;
    bool muted_;
    std::string desc_;
};

}
