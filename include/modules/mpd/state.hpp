#pragma once

#include <fmt/format.h>
#include <mpd/client.h>
#include <spdlog/spdlog.h>

#include <condition_variable>
#include <thread>

#include "ALabel.hpp"

namespace waybar::modules {
class MPD;
}  // namespace waybar::modules

namespace waybar::modules::detail {

using unique_connection = std::unique_ptr<mpd_connection, decltype(&mpd_connection_free)>;
using unique_status = std::unique_ptr<mpd_status, decltype(&mpd_status_free)>;
using unique_song = std::unique_ptr<mpd_song, decltype(&mpd_song_free)>;

class Context;

/// This state machine loosely follows a non-hierarchical, statechart
/// pattern, and includes ENTRY and EXIT actions.
///
/// The State class is the base class for all other states. The
/// entry and exit methods are automatically called when entering
/// into a new state and exiting from the current state. This
/// includes initially entering (Disconnected class) and exiting
/// Waybar.
///
/// The following nested "top-level" states are represented:
/// 1. Idle - await notification of MPD activity.
/// 2. All Non-Idle states:
///    1. Playing - An active song is producing audio output.
///    2. Paused - The current song is paused.
///    3. Stopped - No song is actively playing.
/// 3. Disconnected - periodically attempt MPD (re-)connection.
///
/// NOTE: Since this statechart is non-hierarchical, the above
/// states are flattened into a set.

class State {
 public:
  virtual ~State() noexcept = default;

  virtual void entry() noexcept { spdlog::debug("mpd: ignore entry action"); }
  virtual void exit() noexcept { spdlog::debug("mpd: ignore exit action"); }

  virtual void play() { spdlog::debug("mpd: ignore play state transition"); }
  virtual void stop() { spdlog::debug("mpd: ignore stop state transition"); }
  virtual void pause() { spdlog::debug("mpd: ignore pause state transition"); }

  /// Request state update the GUI.
  virtual void update() noexcept { spdlog::debug("mpd: ignoring update method request"); }
};

class Idle : public State {
  Context* const ctx_;
  sigc::connection idle_connection_;

 public:
  Idle(Context* const ctx) : ctx_{ctx} {}
  virtual ~Idle() noexcept { this->exit(); };

  void entry() noexcept override;
  void exit() noexcept override;

  void play() override;
  void stop() override;
  void pause() override;
  void update() noexcept override;

 private:
  Idle(const Idle&) = delete;
  Idle& operator=(const Idle&) = delete;

  bool on_io(Glib::IOCondition const&);
};

class Playing : public State {
  Context* const ctx_;
  sigc::connection timer_connection_;

 public:
  Playing(Context* const ctx) : ctx_{ctx} {}
  virtual ~Playing() noexcept { this->exit(); }

  void entry() noexcept override;
  void exit() noexcept override;

  void pause() override;
  void stop() override;
  void update() noexcept override;

 private:
  Playing(Playing const&) = delete;
  Playing& operator=(Playing const&) = delete;

  bool on_timer();
};

class Paused : public State {
  Context* const ctx_;
  sigc::connection timer_connection_;

 public:
  Paused(Context* const ctx) : ctx_{ctx} {}
  virtual ~Paused() noexcept { this->exit(); }

  void entry() noexcept override;
  void exit() noexcept override;

  void play() override;
  void stop() override;
  void update() noexcept override;

 private:
  Paused(Paused const&) = delete;
  Paused& operator=(Paused const&) = delete;

  bool on_timer();
};

class Stopped : public State {
  Context* const ctx_;
  sigc::connection timer_connection_;

 public:
  Stopped(Context* const ctx) : ctx_{ctx} {}
  virtual ~Stopped() noexcept { this->exit(); }

  void entry() noexcept override;
  void exit() noexcept override;

  void play() override;
  void pause() override;
  void update() noexcept override;

 private:
  Stopped(Stopped const&) = delete;
  Stopped& operator=(Stopped const&) = delete;

  bool on_timer();
};

class Disconnected : public State {
  Context* const ctx_;
  sigc::connection timer_connection_;

 public:
  Disconnected(Context* const ctx) : ctx_{ctx} {}
  virtual ~Disconnected() noexcept { this->exit(); }

  void entry() noexcept override;
  void exit() noexcept override;

  void update() noexcept override;

 private:
  Disconnected(Disconnected const&) = delete;
  Disconnected& operator=(Disconnected const&) = delete;

  void arm_timer(int interval) noexcept;
  void disarm_timer() noexcept;

  bool on_timer();
};

class Context {
  std::unique_ptr<State> state_;
  waybar::modules::MPD* mpd_module_;

  friend class State;
  friend class Playing;
  friend class Paused;
  friend class Stopped;
  friend class Disconnected;
  friend class Idle;

 protected:
  void setState(std::unique_ptr<State>&& new_state) noexcept {
    if (state_.get() != nullptr) {
      state_->exit();
    }
    state_ = std::move(new_state);
    state_->entry();
  }

  bool is_connected() const;
  bool is_playing() const;
  bool is_paused() const;
  bool is_stopped() const;
  constexpr std::size_t interval() const;
  void tryConnect() const;
  void checkErrors(mpd_connection*) const;
  void do_update();
  void queryMPD() const;
  void fetchState() const;
  constexpr mpd_state state() const;
  void emit() const;
  [[nodiscard]] unique_connection& connection();

 public:
  explicit Context(waybar::modules::MPD* const mpd_module)
      : state_{std::make_unique<Disconnected>(this)}, mpd_module_{mpd_module} {
    state_->entry();
  }

  void play() { state_->play(); }
  void stop() { state_->stop(); }
  void pause() { state_->pause(); }
  void update() noexcept { state_->update(); }
};

}  // namespace waybar::modules::detail
