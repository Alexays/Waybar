#pragma once

#include <glibmm/dispatcher.h>
#include <sigc++/signal.h>

#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>

namespace waybar {

/**
 * Thread-safe signal wrapper.
 * Uses Glib::Dispatcher to pass events to another thread and locked queue to pass the arguments.
 */
template <typename... Args>
struct SafeSignal : sigc::signal<void(std::decay_t<Args>...)> {
 public:
  SafeSignal() { dp_.connect(sigc::mem_fun(*this, &SafeSignal::handle_event)); }

  template <typename... EmitArgs>
  void emit(EmitArgs&&... args) {
    if (main_tid_ == std::this_thread::get_id()) {
      /*
       * Bypass the queue if the method is called the main thread.
       * Ensures that events emitted from the main thread are processed synchronously and saves a
       * few CPU cycles on locking/queuing.
       * As a downside, this makes main thread events prioritized over the other threads and
       * disrupts chronological order.
       */
      signal_t::emit(std::forward<EmitArgs>(args)...);
    } else {
      {
        std::unique_lock lock(mutex_);
        queue_.emplace(std::forward<EmitArgs>(args)...);
      }
      dp_.emit();
    }
  }

  template <typename... EmitArgs>
  inline void operator()(EmitArgs&&... args) {
    emit(std::forward<EmitArgs>(args)...);
  }

 protected:
  using signal_t = sigc::signal<void(std::decay_t<Args>...)>;
  using slot_t = decltype(std::declval<signal_t>().make_slot());
  using arg_tuple_t = std::tuple<std::decay_t<Args>...>;
  // ensure that unwrapped methods are not accessible
  using signal_t::emit_reverse;
  using signal_t::make_slot;

  void handle_event() {
    for (std::unique_lock lock(mutex_); !queue_.empty(); lock.lock()) {
      auto args = queue_.front();
      queue_.pop();
      lock.unlock();
      std::apply(cached_fn_, args);
    }
  }

  Glib::Dispatcher dp_;
  std::mutex mutex_;
  std::queue<arg_tuple_t> queue_;
  const std::thread::id main_tid_ = std::this_thread::get_id();
  // cache functor for signal emission to avoid recreating it on each event
  const slot_t cached_fn_ = make_slot();
};

}  // namespace waybar
