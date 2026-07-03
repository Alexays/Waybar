#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <thread>

#include "util/sleeper_thread.hpp"

namespace waybar::util {
SafeSignal<bool>& prepare_for_sleep() {
  static SafeSignal<bool> signal;
  return signal;
}
}  // namespace waybar::util

namespace {
int run_in_subprocess(int (*task)()) {
  const auto pid = fork();
  if (pid < 0) {
    return -1;
  }
  if (pid == 0) {
    alarm(5);
    _exit(task());
  }

  int status = -1;
  if (waitpid(pid, &status, 0) != pid) {
    return -1;
  }
  if (!WIFEXITED(status)) {
    return -1;
  }
  return WEXITSTATUS(status);
}

int run_reassignment_regression() {
  waybar::util::SleeperThread thread;
  thread = [] { std::this_thread::sleep_for(std::chrono::milliseconds(10)); };
  thread = [] { std::this_thread::sleep_for(std::chrono::milliseconds(1)); };
  return 0;
}

int run_control_flag_stress() {
  for (int i = 0; i < 200; ++i) {
    waybar::util::SleeperThread thread;
    thread = [&thread] { thread.sleep_for(std::chrono::milliseconds(1)); };

    std::thread waker([&thread] {
      for (int j = 0; j < 100; ++j) {
        thread.wake_up();
        std::this_thread::yield();
      }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    thread.stop();
    waker.join();
    if (thread.isRunning()) {
      return 1;
    }
  }
  return 0;
}
}  // namespace

TEST_CASE("SleeperThread reassignment does not terminate process", "[util][sleeper_thread]") {
  REQUIRE(run_in_subprocess(run_reassignment_regression) == 0);
}

TEST_CASE("SleeperThread control flags are stable under concurrent wake and stop",
          "[util][sleeper_thread]") {
  REQUIRE(run_in_subprocess(run_control_flag_stress) == 0);
}
