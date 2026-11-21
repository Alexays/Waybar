#pragma once

#include <glibmm/dispatcher.h>
#include <sigc++/signal.h>

#include <atomic>
#include <utility>

namespace waybar::util {

// Glib::Dispatcher with at most one notification in flight: emit() writes to a bounded pipe that
// only the main loop drains, so one write per event lets a burst block the main thread forever.
class CoalescingDispatcher {
 public:
  // A dispatcher created pending ignores emit() until its owner calls dispatchPending().
  explicit CoalescingDispatcher(bool pending = false) : pending_(pending) {
    dp_.connect([this] { dispatchPending(); });
  }

  void emit() {
    if (!pending_.exchange(true)) {
      dp_.emit();
    }
  }

  // Runs the slots on the calling (main) thread if an emit() is pending.
  void dispatchPending() {
    // Reset before running the slots, so an emit() from a slot schedules another run.
    if (pending_.exchange(false)) {
      signal_.emit();
    }
  }

  template <typename Slot>
  sigc::connection connect(Slot&& slot) {
    return signal_.connect(std::forward<Slot>(slot));
  }

 private:
  sigc::signal<void()> signal_;
  std::atomic<bool> pending_{false};
  // Last member: destroyed first, so its handler never outlives signal_ or pending_.
  Glib::Dispatcher dp_;
};

}  // namespace waybar::util
