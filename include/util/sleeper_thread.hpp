#pragma once

#include <chrono>
#include <condition_variable>
#include <ctime>
#include <functional>
#include <thread>

#include "prepare_for_sleep.h"

namespace waybar::util {

/**
 * Defer pthread_cancel until the end of a current scope.
 *
 * Required to protect a scope where it's unsafe to raise `__forced_unwind` exception.
 * An example of these is a call of a method marked as `noexcept`; an attempt to cancel within such
 * a method may result in a `std::terminate` call.
 */
class CancellationGuard {
  int oldstate;

 public:
  CancellationGuard() { pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate); }
  ~CancellationGuard() { pthread_setcancelstate(oldstate, &oldstate); }
};

class SleeperThread {
 public:
  SleeperThread() = default;

  SleeperThread(std::function<void()> func)
      : thread_{[this, func] {
          while (do_run_) {
            signal_ = false;
            func();
          }
        }} {
    connection_ = prepare_for_sleep().connect([this](bool sleep) {
      if (not sleep) wake_up();
    });
  }

  SleeperThread& operator=(std::function<void()> func) {
    thread_ = std::thread([this, func] {
      while (do_run_) {
        signal_ = false;
        func();
      }
    });
    if (connection_.empty()) {
      connection_ = prepare_for_sleep().connect([this](bool sleep) {
        if (not sleep) wake_up();
      });
    }
    return *this;
  }

  bool isRunning() const { return do_run_; }

  auto sleep() {
    std::unique_lock lk(mutex_);
    CancellationGuard cancel_lock;
    return condvar_.wait(lk, [this] { return signal_ || !do_run_; });
  }

  auto sleep_for(std::chrono::system_clock::duration dur) {
    std::unique_lock lk(mutex_);
    CancellationGuard cancel_lock;
    constexpr auto max_time_point = std::chrono::steady_clock::time_point::max();
    auto wait_end = max_time_point;
    auto now = std::chrono::steady_clock::now();
    if (now < max_time_point - dur) {
      wait_end = now + dur;
    }
    return condvar_.wait_until(lk, wait_end, [this] { return signal_ || !do_run_; });
  }

  auto sleep_until(
      std::chrono::time_point<std::chrono::system_clock, std::chrono::system_clock::duration>
          time_point) {
    std::unique_lock lk(mutex_);
    CancellationGuard cancel_lock;
    return condvar_.wait_until(lk, time_point, [this] { return signal_ || !do_run_; });
  }

  void wake_up() {
    {
      std::lock_guard<std::mutex> lck(mutex_);
      signal_ = true;
    }
    condvar_.notify_all();
  }

  auto stop() {
    {
      std::lock_guard<std::mutex> lck(mutex_);
      signal_ = true;
      do_run_ = false;
    }
    condvar_.notify_all();
    auto handle = thread_.native_handle();
    if (handle != 0) {
      // TODO: find a proper way to terminate thread...
      pthread_cancel(handle);
    }
  }

  ~SleeperThread() {
    connection_.disconnect();
    stop();
    if (thread_.joinable()) {
      thread_.join();
    }
  }

 private:
  std::thread thread_;
  std::condition_variable condvar_;
  std::mutex mutex_;
  bool do_run_ = true;
  bool signal_ = false;
  sigc::connection connection_;
};

}  // namespace waybar::util
