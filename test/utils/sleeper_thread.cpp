#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <fcntl.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/sink.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>
#include <thread>

#include "util/cancel_safe_sink.hpp"
#include "util/scoped_fd.hpp"
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

int run_explicit_join_regression() {
  std::promise<void> entered;
  std::atomic<bool> exited{false};
  waybar::util::SleeperThread thread;
  thread = [&] {
    struct ExitMarker {
      std::atomic<bool>& exited;
      ~ExitMarker() { exited.store(true); }
    } marker{exited};
    entered.set_value();
    thread.sleep_for(std::chrono::hours(1));
  };
  entered.get_future().wait();
  thread.stop();
  thread.join();
  // The callback must have unwound before its resources can be released.
  return exited.load() ? 0 : 1;
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

int run_stop_from_own_thread() {
  std::atomic<bool> done{false};
  waybar::util::SleeperThread thread;
  thread = [&thread, &done] {
    {
      // ~ScopedFd() calls close(), a cancellation point, from a noexcept destructor.
      waybar::util::ScopedFd fd(open("/dev/null", O_RDONLY | O_CLOEXEC));
      thread.stop();
    }
    done.store(true);
  };
  while (!done.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return 0;
}

// A sink that is slow to write, like a busy console or journal. usleep() is a cancellation
// point, as write() is.
struct SlowSink final : spdlog::sinks::sink {
  std::atomic<bool> entered{false};
  std::atomic<bool> finished{false};
  void log(const spdlog::details::log_msg& /*msg*/) override {
    entered.store(true);
    usleep(300 * 1000);
    finished.store(true);
  }
  void flush() override {}
  void set_pattern(const std::string& /*pattern*/) override {}
  void set_formatter(std::unique_ptr<spdlog::formatter> /*formatter*/) override {}
};

int run_stop_while_logging_from_catch() {
  auto sink = std::make_shared<SlowSink>();
  spdlog::logger logger("sleeper_thread_test", sink);
  waybar::util::makeLoggerCancelSafe(logger);
  waybar::util::SleeperThread thread;
  thread = [&thread, &logger] {
    try {
      throw std::runtime_error("connection lost");
    } catch (const std::exception& e) {
      // Module workers log their errors from catch blocks
      logger.error("worker: {}", e.what());
    }
    thread.sleep_for(std::chrono::hours(1));
  };
  while (!sink->entered.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  // pthread_cancel() while the worker is inside the sink, within its catch block
  thread.stop();
  thread.join();
  return sink->finished.load() ? 0 : 1;
}
}  // namespace

TEST_CASE("SleeperThread reassignment does not terminate process", "[util][sleeper_thread]") {
  REQUIRE(run_in_subprocess(run_reassignment_regression) == 0);
}

TEST_CASE("SleeperThread can be joined before its owner releases resources",
          "[util][sleeper_thread]") {
  REQUIRE(run_in_subprocess(run_explicit_join_regression) == 0);
}

TEST_CASE("SleeperThread control flags are stable under concurrent wake and stop",
          "[util][sleeper_thread]") {
  REQUIRE(run_in_subprocess(run_control_flag_stress) == 0);
}

TEST_CASE("SleeperThread stopping itself does not terminate process", "[util][sleeper_thread]") {
  REQUIRE(run_in_subprocess(run_stop_from_own_thread) == 0);
}

TEST_CASE("SleeperThread stopped while its worker logs from a catch block does not terminate",
          "[util][sleeper_thread]") {
  REQUIRE(run_in_subprocess(run_stop_while_logging_from_catch) == 0);
}
