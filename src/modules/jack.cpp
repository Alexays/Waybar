#include "modules/jack.hpp"

namespace waybar::modules {

JACK::JACK(const std::string &id, const Json::Value &config)
    : ALabel(config, "jack", id, "{load}%", 1) {
  running_ = false;
  client_ = NULL;

  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

std::string JACK::JACKState() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (running_) {
    load_ = jack_cpu_load(client_);
    return state_;
  }

  xruns_ = 0;
  load_ = 0;
  bufsize_ = 0;
  samplerate_ = 0;

  if (client_) {
    jack_client_close(client_);
    client_ = NULL;
  }

  client_ = jack_client_open("waybar", JackNoStartServer, NULL);
  if (!client_) return "disconnected";

  if (config_["realtime"].isBool() && !config_["realtime"].asBool()) {
    pthread_t jack_thread = jack_client_thread_id(client_);
    jack_drop_real_time_scheduling(jack_thread);
  }

  bufsize_ = jack_get_buffer_size(client_);
  samplerate_ = jack_get_sample_rate(client_);
  jack_set_sample_rate_callback(client_, sampleRateCallback, this);
  jack_set_buffer_size_callback(client_, bufSizeCallback, this);
  jack_set_xrun_callback(client_, xrunCallback, this);
  jack_on_shutdown(client_, shutdownCallback, this);
  if (jack_activate(client_)) return "disconnected";

  running_ = true;
  return "connected";
}

auto JACK::update() -> void {
  std::string format;
  std::string state = JACKState();
  float latency = 1000 * (float)bufsize_ / (float)samplerate_;

  if (label_.get_style_context()->has_class("xrun")) {
    label_.get_style_context()->remove_class("xrun");
    state = "connected";
  }

  if (label_.get_style_context()->has_class(state_))
    label_.get_style_context()->remove_class(state_);
  label_.get_style_context()->add_class(state);
  state_ = state;

  if (config_["format-" + state].isString()) {
    format = config_["format-" + state].asString();
  } else if (config_["format"].isString()) {
    format = config_["format"].asString();
  } else
    format = "{load}%";

  label_.set_markup(fmt::format(fmt::runtime(format), fmt::arg("load", std::round(load_)),
                                fmt::arg("bufsize", bufsize_), fmt::arg("samplerate", samplerate_),
                                fmt::arg("latency", fmt::format("{:.2f}", latency)),
                                fmt::arg("xruns", xruns_)));

  if (tooltipEnabled()) {
    std::string tooltip_format = "{bufsize}/{samplerate} {latency}ms";
    if (config_["tooltip-format"].isString()) tooltip_format = config_["tooltip-format"].asString();
    label_.set_tooltip_text(fmt::format(
        fmt::runtime(tooltip_format), fmt::arg("load", std::round(load_)),
        fmt::arg("bufsize", bufsize_), fmt::arg("samplerate", samplerate_),
        fmt::arg("latency", fmt::format("{:.2f}", latency)), fmt::arg("xruns", xruns_)));
  }

  // Call parent update
  ALabel::update();
}

int JACK::bufSize(jack_nframes_t size) {
  bufsize_ = size;
  return 0;
}

int JACK::sampleRate(jack_nframes_t rate) {
  samplerate_ = rate;
  return 0;
}

int JACK::xrun() {
  xruns_ += 1;
  state_ = "xrun";
  return 0;
}

void JACK::shutdown() {
  std::lock_guard<std::mutex> lock(mutex_);
  running_ = false;
}

}  // namespace waybar::modules

int bufSizeCallback(jack_nframes_t size, void *obj) {
  return static_cast<waybar::modules::JACK *>(obj)->bufSize(size);
}

int sampleRateCallback(jack_nframes_t rate, void *obj) {
  return static_cast<waybar::modules::JACK *>(obj)->sampleRate(rate);
}

int xrunCallback(void *obj) { return static_cast<waybar::modules::JACK *>(obj)->xrun(); }

void shutdownCallback(void *obj) { return static_cast<waybar::modules::JACK *>(obj)->shutdown(); }
