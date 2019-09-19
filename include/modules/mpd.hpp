#pragma once

#include <fmt/format.h>
#include <mpd/client.h>
#include <condition_variable>
#include <thread>
#include "ALabel.hpp"

namespace waybar::modules {

class MPD : public ALabel {
 public:
  MPD(const std::string&, const Json::Value&);
  auto update() -> void;

 private:
  std::thread periodic_updater();
  std::string getTag(mpd_tag_type type, unsigned idx = 0);
  void        setLabel();
  std::string getStateIcon();
  std::string getOptionIcon(std::string optionName, bool activated);

  std::thread event_listener();

  // Assumes `connection_lock_` is locked
  void tryConnect();
  // If checking errors on the main connection, make sure to lock it using
  // `connection_lock_` before calling checkErrors
  void checkErrors(mpd_connection* conn);

  // Assumes `connection_lock_` is locked
  void fetchState();
  void waitForEvent();

  bool handlePlayPause(GdkEventButton* const&);

  bool stopped();
  bool playing();

  const std::string module_name_;

  using unique_connection = std::unique_ptr<mpd_connection, decltype(&mpd_connection_free)>;
  using unique_status = std::unique_ptr<mpd_status, decltype(&mpd_status_free)>;
  using unique_song = std::unique_ptr<mpd_song, decltype(&mpd_song_free)>;

  // Not using unique_ptr since we don't manage the pointer
  // (It's either nullptr, or from the config)
  const char*    server_;
  const unsigned port_;

  unsigned timeout_;

  // We need a mutex here because we can trigger updates from multiple thread:
  // the event based updates, the periodic updates needed for the elapsed time,
  // and the click play/pause feature
  std::mutex        connection_lock_;
  unique_connection connection_;
  // The alternate connection will be used to wait for events: since it will
  // be blocking (idle) we can't send commands via this connection
  //
  // No lock since only used in the event listener thread
  unique_connection alternate_connection_;

  // Protect them using the `connection_lock_`
  unique_status status_;
  mpd_state     state_;
  unique_song   song_;

  // To make sure the previous periodic_updater stops before creating a new one
  std::mutex periodic_lock_;
};

}  // namespace waybar::modules
