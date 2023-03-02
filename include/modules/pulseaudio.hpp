#pragma once

#include <fmt/format.h>
#include <pulse/pulseaudio.h>
#include <pulse/volume.h>

#include <algorithm>
#include <array>

#include "ALabel.hpp"

namespace waybar::modules {

class Pulseaudio : public ALabel {
 public:
  Pulseaudio(const std::string&, const Json::Value&);
  virtual ~Pulseaudio();
  auto update() -> void override;

 private:
  static void subscribeCb(pa_context*, pa_subscription_event_type_t, uint32_t, void*);
  static void contextStateCb(pa_context*, void*);
  static void sinkInfoCb(pa_context*, const pa_sink_info*, int, void*);
  static void sourceInfoCb(pa_context*, const pa_source_info* i, int, void* data);
  static void serverInfoCb(pa_context*, const pa_server_info*, void*);
  static void volumeModifyCb(pa_context*, int, void*);

  bool handleScroll(GdkEventScroll* e) override;
  const std::vector<std::string> getPulseIcon() const;

  pa_threaded_mainloop* mainloop_;
  pa_mainloop_api* mainloop_api_;
  pa_context* context_;
  // SINK
  uint32_t sink_idx_{0};
  uint16_t volume_;
  pa_cvolume pa_volume_;
  bool muted_;
  std::string port_name_;
  std::string form_factor_;
  std::string desc_;
  std::string monitor_;
  std::string current_sink_name_;
  bool current_sink_running_;
  // SOURCE
  uint32_t source_idx_{0};
  uint16_t source_volume_;
  bool source_muted_;
  std::string source_port_name_;
  std::string source_desc_;
  std::string default_source_name_;
};

}  // namespace waybar::modules
