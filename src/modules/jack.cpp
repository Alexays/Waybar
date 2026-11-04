#include "modules/jack.hpp"

namespace waybar::modules {

JACK::JACK(const std::string& id, const Json::Value& config)
    : ALabel(config, "jack", id, "{load}%", 1) {
  // jack_client_open() can block for as long as whoever holds the server socket wants, so the
  // libjack calls are made here and never on the GTK main loop.
  thread_ = [this] {
    {
      // A cancellation inside libjack would unwind through JACK2's catch (...) and abort.
      util::CancellationGuard cancel_guard;
      JACKState();
    }
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

void JACK::JACKState() {
  std::unique_lock<std::mutex> lock(mutex_);
  if (running_) {
    load_ = jack_cpu_load(client_.get());
    return;
  }

  xruns_ = 0;
  load_ = 0;
  bufsize_ = 0;
  samplerate_ = 0;
  state_ = "disconnected";

  // The callbacks take mutex_, and libjack runs them from the calls below: JACK2 calls the sample
  // rate one inside jack_set_sample_rate_callback(), jack_client_close() joins their thread.
  lock.unlock();

  client_.reset();
  client_.reset(jack_client_open("waybar", JackNoStartServer, NULL));
  if (!client_) return;

  if (config_["realtime"].isBool() && !config_["realtime"].asBool()) {
    pthread_t jack_thread = jack_client_thread_id(client_.get());
    jack_drop_real_time_scheduling(jack_thread);
  }

  auto bufsize = jack_get_buffer_size(client_.get());
  auto samplerate = jack_get_sample_rate(client_.get());
  lock.lock();
  bufsize_ = bufsize;
  samplerate_ = samplerate;
  // Set before activating, so a shutdown() delivered right after is not overwritten.
  running_ = true;
  state_ = "connected";
  lock.unlock();

  jack_set_sample_rate_callback(client_.get(), sampleRateCallback, this);
  jack_set_buffer_size_callback(client_.get(), bufSizeCallback, this);
  jack_set_xrun_callback(client_.get(), xrunCallback, this);
  jack_on_shutdown(client_.get(), shutdownCallback, this);
  if (jack_activate(client_.get())) {
    shutdown();
  }
}

auto JACK::update() -> void {
  std::string state;
  jack_nframes_t bufsize;
  jack_nframes_t samplerate;
  unsigned int xruns;
  float load;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    state = state_;
    // An xrun is shown for one update.
    if (state_ == "xrun") state_ = "connected";
    bufsize = bufsize_;
    samplerate = samplerate_;
    xruns = xruns_;
    load = load_;
  }
  float latency = samplerate > 0 ? 1000.0f * (float)bufsize / (float)samplerate : 0.0f;

  auto style = label_.get_style_context();
  style->remove_class("connected");
  style->remove_class("disconnected");
  style->remove_class("xrun");
  style->add_class(state);

  std::string format;
  if (config_["format-" + state].isString()) {
    format = config_["format-" + state].asString();
  } else if (config_["format"].isString()) {
    format = config_["format"].asString();
  } else
    format = "{load}%";

  updateLabelAndTooltip(
      format, "{bufsize}/{samplerate} {latency}ms", fmt::arg("load", std::round(load)),
      fmt::arg("bufsize", bufsize), fmt::arg("samplerate", samplerate),
      fmt::arg("latency", fmt::format("{:.2f}", latency)), fmt::arg("xruns", xruns));

  // Call parent update
  ALabel::update();
}

int JACK::bufSize(jack_nframes_t size) {
  std::lock_guard<std::mutex> lock(mutex_);
  bufsize_ = size;
  return 0;
}

int JACK::sampleRate(jack_nframes_t rate) {
  std::lock_guard<std::mutex> lock(mutex_);
  samplerate_ = rate;
  return 0;
}

int JACK::xrun() {
  std::lock_guard<std::mutex> lock(mutex_);
  xruns_ += 1;
  state_ = "xrun";
  return 0;
}

void JACK::shutdown() {
  std::lock_guard<std::mutex> lock(mutex_);
  running_ = false;
  state_ = "disconnected";
}

}  // namespace waybar::modules

int bufSizeCallback(jack_nframes_t size, void* obj) {
  return static_cast<waybar::modules::JACK*>(obj)->bufSize(size);
}

int sampleRateCallback(jack_nframes_t rate, void* obj) {
  return static_cast<waybar::modules::JACK*>(obj)->sampleRate(rate);
}

int xrunCallback(void* obj) { return static_cast<waybar::modules::JACK*>(obj)->xrun(); }

void shutdownCallback(void* obj) { return static_cast<waybar::modules::JACK*>(obj)->shutdown(); }
