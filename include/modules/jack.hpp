#pragma once

#include <fmt/format.h>
#include <jack/jack.h>
#include <jack/thread.h>

#include <fstream>
#include <memory>
#include <mutex>
#include <string>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class JACK : public ALabel {
 public:
  JACK(const std::string&, const Json::Value&);
  virtual ~JACK() = default;
  auto update() -> void override;

  int bufSize(jack_nframes_t size);
  int sampleRate(jack_nframes_t rate);
  int xrun();
  void shutdown();

 private:
  void JACKState();

  // Guarded by mutex_.
  jack_nframes_t bufsize_ = 0;
  jack_nframes_t samplerate_ = 0;
  unsigned int xruns_ = 0;
  float load_ = 0;
  bool running_ = false;
  std::string state_ = "disconnected";
  std::mutex mutex_;
  // Only used by thread_. Declared after what its callbacks use and before thread_, so it is
  // closed once thread_ is joined and while the callbacks can still run.
  std::unique_ptr<jack_client_t, decltype(&jack_client_close)> client_{nullptr, &jack_client_close};
  util::SleeperThread thread_;
};

}  // namespace waybar::modules

int bufSizeCallback(jack_nframes_t size, void* obj);
int sampleRateCallback(jack_nframes_t rate, void* obj);
int xrunCallback(void* obj);
void shutdownCallback(void* obj);
