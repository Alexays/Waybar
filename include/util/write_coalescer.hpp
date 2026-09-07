#pragma once

#include <sigc++/connection.h>

#include <chrono>
#include <functional>
#include <optional>

namespace waybar::util {

enum class CoalesceMode {
  THROTTLE,  // leading write, then <=1 write per min_gap, plus a trailing final write
  DEBOUNCE,  // write only after min_gap of silence
};

/**
 * Rate-limits a stream of values to bounded writes; GTK-thread only, writer is
 * synchronous so single-flight is trivial. Properties: leading edge (THROTTLE),
 * coalescing (newest wins, never queued), convergence (final value always written).
 */
class WriteCoalescer {
 public:
  using Clock = std::chrono::steady_clock;

  WriteCoalescer(std::function<void(int)> writer, std::chrono::milliseconds min_gap,
                 CoalesceMode mode);
  ~WriteCoalescer();

  // Feed a new target value; may write immediately under THROTTLE's leading edge.
  void submit(int value);
  // Write any pending value now and cancel the timer (e.g. on drag end).
  void flush();

  // Injectable clock and manual timer fire, for tests without a GLib main loop.
  void setClock(std::function<Clock::time_point()> now) { now_ = std::move(now); }
  void tick();

 private:
  void arm();
  void writeNow(int value, Clock::time_point now);

  std::function<void(int)> writer_;
  std::chrono::milliseconds min_gap_;
  CoalesceMode mode_;
  std::function<Clock::time_point()> now_ = [] { return Clock::now(); };

  std::optional<int> pending_;
  std::optional<Clock::time_point> deadline_;
  bool have_written_ = false;
  int last_written_ = 0;
  Clock::time_point last_write_time_;
  sigc::connection timer_;
};

}  // namespace waybar::util
