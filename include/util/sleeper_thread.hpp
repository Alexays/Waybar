#pragma once

#include <chrono>
#include <condition_variable>
#include <ctime>
#include <functional>
#include <thread>

namespace waybar::util {

class SleeperThread {
 public:
  SleeperThread() = default;

  SleeperThread(std::function<void()> func)
      : thread_{[this, func] {
          while (do_run_) {
            signal_ = false;
            func();
          }
        }} {}

  SleeperThread& operator=(std::function<void()> func) {
    thread_ = std::thread([this, func] {
      while (do_run_) {
        signal_ = false;
        func();
      }
    });
    return *this;
  }

  bool isRunning() const { return do_run_; }

  auto sleep_for(std::chrono::system_clock::duration dur) {
    std::unique_lock lk(mutex_);
    return condvar_.wait_for(lk, dur, [this] { return signal_ || !do_run_; });
  }

  auto sleep_until(
      std::chrono::time_point<std::chrono::system_clock, std::chrono::system_clock::duration>
          time_point) {
    std::unique_lock lk(mutex_);
    return condvar_.wait_until(lk, time_point, [this] { return signal_ || !do_run_; });
  }

  auto wake_up() {
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
  }

  ~SleeperThread() {
    stop();
    if (thread_.joinable()) {
      thread_.join();
    }
  }

 private:
  std::thread             thread_;
  std::condition_variable condvar_;
  std::mutex              mutex_;
  bool                    do_run_ = true;
  bool                    signal_ = false;
};

}  // namespace waybar::util
