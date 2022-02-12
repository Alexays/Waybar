#include "modules/jack.hpp"

waybar::modules::JACK::JACK(const std::string& id, const Json::Value& config)
    : ALabel(config, "jack", id, "{load}%", 1) {
  xruns_  = 0;
  state_  = "disconnected";
  client_ = NULL;

  state_  = JACKState();
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

std::string waybar::modules::JACK::JACKState() {
  if(state_.compare("xrun") == 0)
    return "xrun";
  if(state_.compare("connected") == 0)
    return "connected";

  std::string procname;
  bool foundJACK = false;
  proc_t** proctab = readproctab(PROC_FILLSTAT);
  for(int i=0; proctab[i]; i++) {
    procname = proctab[i]->cmd;
    if(!procname.compare("jackd") || !procname.compare("pipewire"))
      foundJACK = true;
    freeproc(proctab[i]);
  }
  free(proctab);
  if(!foundJACK)
    return "disconnected";

  client_ = jack_client_open("waybar", JackNoStartServer, NULL);
  if (client_) {
    jack_thread_ = jack_client_thread_id(client_);
    if(config_["realtime"].isBool() && !config_["realtime"].asBool())
      jack_drop_real_time_scheduling(jack_thread_);

    bufsize_ = jack_get_buffer_size(client_);
    samplerate_ = jack_get_sample_rate(client_);
    jack_set_buffer_size_callback(client_, bufSizeCallback, this);
    jack_set_xrun_callback(client_, xrunCallback, this);
    jack_on_shutdown(client_, shutdownCallback, this);

    if (!jack_activate(client_))
      return "connected";
  }
  return "disconnected";
}

auto waybar::modules::JACK::update() -> void {
  std::string format;
  float latency = 1000 * (float)bufsize_ / (float)samplerate_;
  auto state = JACKState();
  float load;

  if(label_.get_style_context()->has_class("xrun")) {
    label_.get_style_context()->remove_class("xrun");
    state = "connected";
  }

  if(state.compare("disconnected") != 0)
    load = jack_cpu_load(client_);
  else {
    load = 0;
    bufsize_ = 0;
    samplerate_ = 0;
    latency = 0;
  }

  if(label_.get_style_context()->has_class(state_))
    label_.get_style_context()->remove_class(state_);

  if (config_["format-" + state].isString()) {
    format = config_["format-" + state].asString();
  } else if (config_["format"].isString()) {
    format = config_["format"].asString();
  } else format = "DSP {load}%";

  if(!label_.get_style_context()->has_class(state))
    label_.get_style_context()->add_class(state);
  state_ = state;

  label_.set_markup(fmt::format(format, fmt::arg("load", std::round(load)),
                                        fmt::arg("bufsize", bufsize_),
                                        fmt::arg("samplerate", samplerate_),
                                        fmt::arg("latency", fmt::format("{:.2f}", latency)),
                                        fmt::arg("xruns", xruns_)));

  if (tooltipEnabled()) {
    std::string tooltip_format = "{bufsize}/{samplerate} {latency}ms";
    if (config_["tooltip-format"].isString())
      tooltip_format = config_["tooltip-format"].asString();
    label_.set_tooltip_text(fmt::format(tooltip_format, fmt::arg("load", std::round(load)),
                                       	fmt::arg("bufsize", bufsize_),
                                       	fmt::arg("samplerate", samplerate_),
                                        fmt::arg("latency", fmt::format("{:.2f}", latency)),
                                        fmt::arg("xruns", xruns_)));

  }

  // Call parent update
  ALabel::update();
}

int bufSizeCallback(unsigned int size, void *obj) {
  waybar::modules::JACK* x = (waybar::modules::JACK*)obj;
  x->bufsize_ = size;
  return size;
}

int xrunCallback(void *obj) {
  waybar::modules::JACK* x = (waybar::modules::JACK*)obj;
  x->xruns_ += 1;
  x->state_ = "xrun";
  return 0;
}

void shutdownCallback(void *obj) {
  waybar::modules::JACK* x = (waybar::modules::JACK*)obj;
  pthread_cancel(x->jack_thread_);
  x->client_ = NULL;
  x->state_ = "disconnected";
  x->xruns_ = 0;
}
