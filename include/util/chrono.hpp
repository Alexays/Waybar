#pragma once

#include <chrono>
#include <ctime>
#include <functional>
#include <condition_variable>
#include <thread>

namespace waybar::chrono {

using namespace std::chrono;

using clock = std::chrono::system_clock;
using duration = clock::duration;
using time_point = std::chrono::time_point<clock, duration>;

}

namespace waybar::util {

struct SleeperThread {
  SleeperThread() = default;

  SleeperThread(std::function<void()> func)
    : do_run_(true), thread_{[this, func] {
        while (do_run_) func();
      }}
  {}

  SleeperThread& operator=(std::function<void()> func)
  {
    do_run_ = true;
    thread_ = std::thread([this, func] {
      while (do_run_) func();
    });
    return *this;
  }

  bool isRunning() const
  {
    return do_run_;
  }

  auto sleep_for(chrono::duration dur)
  {
    auto lock = std::unique_lock(mutex_);
    return condvar_.wait_for(lock, dur);
  }

  auto sleep_until(chrono::time_point time)
  {
    auto lock = std::unique_lock(mutex_);
    return condvar_.wait_until(lock, time);
  }

  auto wake_up()
  {
    condvar_.notify_all();
  }

  auto stop()
  {
    do_run_ = false;
    condvar_.notify_all();
  }

  ~SleeperThread()
  {
    stop();
    if (thread_.joinable()) {
      thread_.detach();
    }
  }

private:
  bool do_run_ = false;
  std::thread thread_;
  std::condition_variable condvar_;
  std::mutex mutex_;
};

}
