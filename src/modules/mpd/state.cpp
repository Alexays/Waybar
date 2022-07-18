#include "modules/mpd/state.hpp"

#include <fmt/chrono.h>
#include <spdlog/spdlog.h>

#include "modules/mpd/mpd.hpp"
#if defined(MPD_NOINLINE)
namespace waybar::modules {
#include "modules/mpd/state.inl.hpp"
}  // namespace waybar::modules
#endif

#if FMT_VERSION >= 90000
/* Satisfy fmt 9.x deprecation of implicit conversion of enums to int */
auto format_as(enum mpd_idle val) {
  return static_cast<std::underlying_type_t<enum mpd_idle>>(val);
}
#endif

namespace waybar::modules::detail {

#define IDLE_RUN_NOIDLE_AND_CMD(...)                                      \
  if (idle_connection_.connected()) {                                     \
    idle_connection_.disconnect();                                        \
    auto conn = ctx_->connection().get();                                 \
    if (!mpd_run_noidle(conn)) {                                          \
      if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {          \
        spdlog::error("mpd: Idle: failed to unregister for IDLE events"); \
        ctx_->checkErrors(conn);                                          \
      }                                                                   \
    }                                                                     \
    __VA_ARGS__;                                                          \
  }

void Idle::play() {
  IDLE_RUN_NOIDLE_AND_CMD(mpd_run_play(conn));

  ctx_->setState(std::make_unique<Playing>(ctx_));
}

void Idle::pause() {
  IDLE_RUN_NOIDLE_AND_CMD(mpd_run_pause(conn, true));

  ctx_->setState(std::make_unique<Paused>(ctx_));
}

void Idle::stop() {
  IDLE_RUN_NOIDLE_AND_CMD(mpd_run_stop(conn));

  ctx_->setState(std::make_unique<Stopped>(ctx_));
}

#undef IDLE_RUN_NOIDLE_AND_CMD

void Idle::update() noexcept {
  // This is intentionally blank.
}

void Idle::entry() noexcept {
  auto conn = ctx_->connection().get();
  assert(conn != nullptr);

  if (!mpd_send_idle_mask(
          conn, static_cast<mpd_idle>(MPD_IDLE_PLAYER | MPD_IDLE_OPTIONS | MPD_IDLE_QUEUE))) {
    ctx_->checkErrors(conn);
    spdlog::error("mpd: Idle: failed to register for IDLE events");
  } else {
    spdlog::debug("mpd: Idle: watching FD");
    sigc::slot<bool, Glib::IOCondition const&> idle_slot = sigc::mem_fun(*this, &Idle::on_io);
    idle_connection_ =
        Glib::signal_io().connect(idle_slot, mpd_connection_get_fd(conn),
                                  Glib::IO_IN | Glib::IO_PRI | Glib::IO_ERR | Glib::IO_HUP);
  }
}

void Idle::exit() noexcept {
  if (idle_connection_.connected()) {
    idle_connection_.disconnect();
    spdlog::debug("mpd: Idle: unwatching FD");
  }
}

bool Idle::on_io(Glib::IOCondition const&) {
  auto conn = ctx_->connection().get();

  // callback should do this:
  enum mpd_idle events = mpd_recv_idle(conn, /* ignore_timeout?= */ false);
  spdlog::debug("mpd: Idle: recv_idle events -> {}", events);

  mpd_response_finish(conn);
  try {
    ctx_->checkErrors(conn);
  } catch (std::exception const& e) {
    spdlog::warn("mpd: Idle: error: {}", e.what());
    ctx_->setState(std::make_unique<Disconnected>(ctx_));
    return false;
  }

  ctx_->fetchState();
  mpd_state state = ctx_->state();

  if (state == MPD_STATE_STOP) {
    ctx_->emit();
    ctx_->setState(std::make_unique<Stopped>(ctx_));
  } else if (state == MPD_STATE_PLAY) {
    ctx_->emit();
    ctx_->setState(std::make_unique<Playing>(ctx_));
  } else if (state == MPD_STATE_PAUSE) {
    ctx_->emit();
    ctx_->setState(std::make_unique<Paused>(ctx_));
  } else {
    ctx_->emit();
    // self transition
    ctx_->setState(std::make_unique<Idle>(ctx_));
  }

  return false;
}

void Playing::entry() noexcept {
  sigc::slot<bool> timer_slot = sigc::mem_fun(*this, &Playing::on_timer);
  timer_connection_ = Glib::signal_timeout().connect(timer_slot, /* milliseconds */ 1'000);
  spdlog::debug("mpd: Playing: enabled 1 second periodic timer.");
}

void Playing::exit() noexcept {
  if (timer_connection_.connected()) {
    timer_connection_.disconnect();
    spdlog::debug("mpd: Playing: disabled 1 second periodic timer.");
  }
}

bool Playing::on_timer() {
  // Attempt to connect with MPD.
  try {
    ctx_->tryConnect();

    // Success?
    if (!ctx_->is_connected()) {
      ctx_->setState(std::make_unique<Disconnected>(ctx_));
      return false;
    }

    ctx_->fetchState();

    if (!ctx_->is_playing()) {
      if (ctx_->is_paused()) {
        ctx_->setState(std::make_unique<Paused>(ctx_));
      } else {
        ctx_->setState(std::make_unique<Stopped>(ctx_));
      }
      return false;
    }

    ctx_->queryMPD();
    ctx_->emit();
  } catch (std::exception const& e) {
    spdlog::warn("mpd: Playing: error: {}", e.what());
    ctx_->setState(std::make_unique<Disconnected>(ctx_));
    return false;
  }

  return true;
}

void Playing::stop() {
  if (timer_connection_.connected()) {
    timer_connection_.disconnect();

    mpd_run_stop(ctx_->connection().get());
  }

  ctx_->setState(std::make_unique<Stopped>(ctx_));
}

void Playing::pause() {
  if (timer_connection_.connected()) {
    timer_connection_.disconnect();

    mpd_run_pause(ctx_->connection().get(), true);
  }

  ctx_->setState(std::make_unique<Paused>(ctx_));
}

void Playing::update() noexcept { ctx_->do_update(); }

void Paused::entry() noexcept {
  sigc::slot<bool> timer_slot = sigc::mem_fun(*this, &Paused::on_timer);
  timer_connection_ = Glib::signal_timeout().connect(timer_slot, /* milliseconds */ 200);
  spdlog::debug("mpd: Paused: enabled 200 ms periodic timer.");
}

void Paused::exit() noexcept {
  if (timer_connection_.connected()) {
    timer_connection_.disconnect();
    spdlog::debug("mpd: Paused: disabled 200 ms periodic timer.");
  }
}

bool Paused::on_timer() {
  bool rc = true;

  // Attempt to connect with MPD.
  try {
    ctx_->tryConnect();

    // Success?
    if (!ctx_->is_connected()) {
      ctx_->setState(std::make_unique<Disconnected>(ctx_));
      return false;
    }

    ctx_->fetchState();

    ctx_->emit();

    if (ctx_->is_paused()) {
      ctx_->setState(std::make_unique<Idle>(ctx_));
      rc = false;
    } else if (ctx_->is_playing()) {
      ctx_->setState(std::make_unique<Playing>(ctx_));
      rc = false;
    } else if (ctx_->is_stopped()) {
      ctx_->setState(std::make_unique<Stopped>(ctx_));
      rc = false;
    }
  } catch (std::exception const& e) {
    spdlog::warn("mpd: Paused: error: {}", e.what());
    ctx_->setState(std::make_unique<Disconnected>(ctx_));
    rc = false;
  }

  return rc;
}

void Paused::play() {
  if (timer_connection_.connected()) {
    timer_connection_.disconnect();

    mpd_run_play(ctx_->connection().get());
  }

  ctx_->setState(std::make_unique<Playing>(ctx_));
}

void Paused::stop() {
  if (timer_connection_.connected()) {
    timer_connection_.disconnect();

    mpd_run_stop(ctx_->connection().get());
  }

  ctx_->setState(std::make_unique<Stopped>(ctx_));
}

void Paused::update() noexcept { ctx_->do_update(); }

void Stopped::entry() noexcept {
  sigc::slot<bool> timer_slot = sigc::mem_fun(*this, &Stopped::on_timer);
  timer_connection_ = Glib::signal_timeout().connect(timer_slot, /* milliseconds */ 200);
  spdlog::debug("mpd: Stopped: enabled 200 ms periodic timer.");
}

void Stopped::exit() noexcept {
  if (timer_connection_.connected()) {
    timer_connection_.disconnect();
    spdlog::debug("mpd: Stopped: disabled 200 ms periodic timer.");
  }
}

bool Stopped::on_timer() {
  bool rc = true;

  // Attempt to connect with MPD.
  try {
    ctx_->tryConnect();

    // Success?
    if (!ctx_->is_connected()) {
      ctx_->setState(std::make_unique<Disconnected>(ctx_));
      return false;
    }

    ctx_->fetchState();

    ctx_->emit();

    if (ctx_->is_stopped()) {
      ctx_->setState(std::make_unique<Idle>(ctx_));
      rc = false;
    } else if (ctx_->is_playing()) {
      ctx_->setState(std::make_unique<Playing>(ctx_));
      rc = false;
    } else if (ctx_->is_paused()) {
      ctx_->setState(std::make_unique<Paused>(ctx_));
      rc = false;
    }
  } catch (std::exception const& e) {
    spdlog::warn("mpd: Stopped: error: {}", e.what());
    ctx_->setState(std::make_unique<Disconnected>(ctx_));
    rc = false;
  }

  return rc;
}

void Stopped::play() {
  if (timer_connection_.connected()) {
    timer_connection_.disconnect();

    mpd_run_play(ctx_->connection().get());
  }

  ctx_->setState(std::make_unique<Playing>(ctx_));
}

void Stopped::pause() {
  if (timer_connection_.connected()) {
    timer_connection_.disconnect();

    mpd_run_pause(ctx_->connection().get(), true);
  }

  ctx_->setState(std::make_unique<Paused>(ctx_));
}

void Stopped::update() noexcept { ctx_->do_update(); }

void Disconnected::arm_timer(int interval) noexcept {
  // unregister timer, if present
  disarm_timer();

  // register timer
  sigc::slot<bool> timer_slot = sigc::mem_fun(*this, &Disconnected::on_timer);
  timer_connection_ = Glib::signal_timeout().connect(timer_slot, interval);
  spdlog::debug("mpd: Disconnected: enabled interval timer.");
}

void Disconnected::disarm_timer() noexcept {
  // unregister timer, if present
  if (timer_connection_.connected()) {
    timer_connection_.disconnect();
    spdlog::debug("mpd: Disconnected: disabled interval timer.");
  }
}

void Disconnected::entry() noexcept {
  ctx_->emit();
  arm_timer(1'000);
}

void Disconnected::exit() noexcept { disarm_timer(); }

bool Disconnected::on_timer() {
  // Attempt to connect with MPD.
  try {
    ctx_->tryConnect();

    // Success?
    if (ctx_->is_connected()) {
      ctx_->fetchState();
      ctx_->emit();

      if (ctx_->is_playing()) {
        ctx_->setState(std::make_unique<Playing>(ctx_));
      } else if (ctx_->is_paused()) {
        ctx_->setState(std::make_unique<Paused>(ctx_));
      } else {
        ctx_->setState(std::make_unique<Stopped>(ctx_));
      }

      return false;  // do not rearm timer
    }
  } catch (std::exception const& e) {
    spdlog::warn("mpd: Disconnected: error: {}", e.what());
  }

  arm_timer(ctx_->interval() * 1'000);

  return false;
}

void Disconnected::update() noexcept { ctx_->do_update(); }

}  // namespace waybar::modules::detail
