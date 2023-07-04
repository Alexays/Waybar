#pragma once

#include <fmt/format.h>
#include <mpd/client.h>
#include <spdlog/spdlog.h>

#include <condition_variable>
#include <thread>

#include "ALabel.hpp"
#include "modules/mpd/state.hpp"

namespace waybar::modules {

class MPD : public ALabel {
  friend class detail::Context;

  // State machine
  detail::Context context_{this};

  const std::string module_name_;

  // Not using unique_ptr since we don't manage the pointer
  // (It's either nullptr, or from the config)
  const char* server_;
  const unsigned port_;
  const std::string password_;

  unsigned timeout_;

  detail::unique_connection connection_;

  detail::unique_status status_;
  mpd_state state_;
  detail::unique_song song_;

 public:
  MPD(const std::string&, const Json::Value&);
  virtual ~MPD() noexcept = default;
  auto update() -> void override;

 private:
  std::string getTag(mpd_tag_type type, unsigned idx = 0) const;
  std::string getFilename() const;
  void setLabel();
  std::string getStateIcon() const;
  std::string getOptionIcon(std::string optionName, bool activated) const;

  // GUI-side methods
  bool handlePlayPause(GdkEventButton* const&);
  void emit() { dp.emit(); }

  // MPD-side, Non-GUI methods.
  void tryConnect();
  void checkErrors(mpd_connection* conn);
  void fetchState();
  void queryMPD();

  inline bool stopped() const { return connection_ && state_ == MPD_STATE_STOP; }
  inline bool playing() const { return connection_ && state_ == MPD_STATE_PLAY; }
  inline bool paused() const { return connection_ && state_ == MPD_STATE_PAUSE; }
};

#if !defined(MPD_NOINLINE)
#include "modules/mpd/state.inl.hpp"
#endif

}  // namespace waybar::modules
