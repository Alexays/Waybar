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

inline struct timespec to_timespec(time_point t) noexcept
{
  long secs = duration_cast<seconds>(t.time_since_epoch()).count();
  long nsc = duration_cast<nanoseconds>(t.time_since_epoch() % seconds(1)).count();
  return {secs, nsc};
}

inline time_point to_time_point(struct timespec t) noexcept
{
  return time_point(duration_cast<duration>(seconds(t.tv_sec) + nanoseconds(t.tv_nsec)));
}

}

namespace waybar::util {

struct SleeperThread {
  SleeperThread() = default;

  SleeperThread(std::function<void()> func)
    : thread{[this, func] {
        do {
          func();
        } while (do_run);
      }}
  {
    defined = true;
  }

  SleeperThread& operator=(std::function<void()> func)
  {
    thread = std::thread([this, func] {
      do {
        func();
      } while (do_run);
    });
    defined = true;
    return *this;
  }


  auto sleep_for(chrono::duration dur)
  {
    auto lock = std::unique_lock(mutex);
    return condvar.wait_for(lock, dur);
  }

  auto sleep_until(chrono::time_point time)
  {
    auto lock = std::unique_lock(mutex);
    return condvar.wait_until(lock, time);
  }

  auto wake_up()
  {
    condvar.notify_all();
  }

  ~SleeperThread()
  {
    do_run = false;
    if (defined) {
      condvar.notify_all();
      thread.join();
    }
  }

private:
  std::thread thread;
  std::condition_variable condvar;
  std::mutex mutex;
  bool defined = false;
  bool do_run = true;
};

}
