#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <thread>

#include "util/sleeper_thread.hpp"

namespace waybar::util {
SafeSignal<bool>& prepare_for_sleep() {
  static SafeSignal<bool> signal;
  return signal;
}
}  // namespace waybar::util

namespace {
// The subprocess watchdog exists to turn a hang into a failure instead of a stalled
// CI job. It must not double as a performance assertion: these cases stress thread
// control flow, and on a loaded builder the whole loop can take several seconds of
// wall clock purely because threads are competing for cores. A short timeout there
// looks exactly like a broken SleeperThread, so the watchdog is generous and its
// expiry is reported as its own exit code rather than as a generic failure.
constexpr int kWatchdogExpired = 124;
constexpr unsigned kWatchdogSeconds = 60;

void watchdogHandler(int) { _exit(kWatchdogExpired); }

const char* subprocessResult(int code) {
  if (code == 0) {
    return "ok";
  }
  if (code == kWatchdogExpired) {
    return "watchdog expired: the stress loop did not finish, which on a loaded "
           "builder means scheduling pressure rather than a logic failure";
  }
  if (code < 0) {
    return "subprocess could not be run or was killed by a signal";
  }
  return "task reported a failure";
}

int run_in_subprocess(int (*task)()) {
  const auto pid = fork();
  if (pid < 0) {
    return -1;
  }
  if (pid == 0) {
    struct sigaction action {};
    action.sa_handler = watchdogHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGALRM, &action, nullptr);
    alarm(kWatchdogSeconds);
    _exit(task());
  }

  int status = -1;
  if (waitpid(pid, &status, 0) != pid) {
    return -1;
  }
  // Default SIGALRM terminates the child, so an expired watchdog used to arrive as
  // "killed by a signal" and was indistinguishable from a crash. The handler above
  // makes it an exit code instead.
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
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
  const int result = run_in_subprocess(run_reassignment_regression);
  INFO("subprocess result " << result << " (" << subprocessResult(result) << ")");
  REQUIRE(result == 0);
}

TEST_CASE("SleeperThread control flags are stable under concurrent wake and stop",
          "[util][sleeper_thread]") {
  const int result = run_in_subprocess(run_control_flag_stress);
  INFO("subprocess result " << result << " (" << subprocessResult(result) << ")");
  REQUIRE(result == 0);
}
