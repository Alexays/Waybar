#pragma once

#include <chrono>
#include <ctime>
#include <functional>
#include <condition_variable>
#include <thread>
#include <gtkmm.h>

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
    : thread_{[this, func] {
        while(true) {
          {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!do_run_) {
              break;
            }
          }
          func();
        }
      }}
  {}

  SleeperThread& operator=(std::function<void()> func)
  {
    thread_ = std::thread([this, func] {
      while(true) {
        {
          std::lock_guard<std::mutex> lock(mutex_);
          if (!do_run_) {
            break;
          }
        }
        func();
      }
    });
    return *this;
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
    if (thread_.joinable()) {
      thread_.detach();
    }
  }

  ~SleeperThread()
  {
    stop();
  }

private:
  std::thread thread_;
  std::condition_variable condvar_;
  std::mutex mutex_;
  bool do_run_ = true;
};

}
