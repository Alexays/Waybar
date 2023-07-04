#pragma once

#include <fmt/format.h>
#include <jack/jack.h>
#include <jack/thread.h>

#include <fstream>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class JACK : public ALabel {
 public:
  JACK(const std::string &, const Json::Value &);
  virtual ~JACK() = default;
  auto update() -> void override;

  int bufSize(jack_nframes_t size);
  int sampleRate(jack_nframes_t rate);
  int xrun();
  void shutdown();

 private:
  std::string JACKState();

  jack_client_t *client_;
  jack_nframes_t bufsize_;
  jack_nframes_t samplerate_;
  unsigned int xruns_;
  float load_;
  bool running_;
  std::mutex mutex_;
  std::string state_;
  util::SleeperThread thread_;
};

}  // namespace waybar::modules

int bufSizeCallback(jack_nframes_t size, void *obj);
int sampleRateCallback(jack_nframes_t rate, void *obj);
int xrunCallback(void *obj);
void shutdownCallback(void *obj);
