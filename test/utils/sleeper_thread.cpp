#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <fcntl.h>
#include <fmt/format.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/sink.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <future>
#include <ostream>
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
// The watchdog turns a hang into a failure instead of a stalled CI job, and it must not
// double as a performance assertion. run_control_flag_stress() makes 200 * 100
// sched_yield() calls, and each yield hands a competing runnable thread a full scheduling
// slice, so its wall time is set by how loaded the machine is rather than by the code:
// 0.46s idle, 68.8s worst of 8 runs with one busy loop per core, 119.3s worst of 5 runs
// with two per core. The budget clears every one of those, so a loaded builder cannot be
// mistaken for a deadlock, while a real deadlock still fails in bounded time.
constexpr unsigned kWatchdogSeconds = 300;

// Task bodies report success with 0 and failure with 1; any other code is a bug in the
// test rather than a result, and is reported as one. The watchdog exits with 124 - the
// code timeout(1) uses for the same meaning - which is outside that set.
constexpr int kTaskFailed = 1;
constexpr int kWatchdogExpired = 124;

void watchdogHandler(int) { _exit(kWatchdogExpired); }

struct SubprocessOutcome {
  enum class Kind {
    passed,
    taskFailed,
    watchdogExpired,
    killedBySignal,
    harnessError,
    unexpectedExit,
  };

  Kind kind;
  int detail;  // signal number, errno, or the unrecognised exit code

  auto describe() const -> std::string {
    switch (kind) {
      case Kind::passed:
        return "the subprocess passed";
      case Kind::taskFailed:
        return fmt::format("the subprocess exited {}: the task itself reported a failure", detail);
      case Kind::watchdogExpired:
        return fmt::format(
            "watchdog expired after {}s: this case did not finish. A loaded machine looks "
            "exactly like this and it is not a SleeperThread defect; treat it as a hang only "
            "if it also expires on an idle machine.",
            kWatchdogSeconds);
      case Kind::killedBySignal:
        return fmt::format(
            "the subprocess was killed by signal {} ({}) before the watchdog "
            "could fire: that is a crash, not a timeout",
            detail, strsignal(detail));
      case Kind::harnessError:
        return fmt::format("could not run the subprocess: {}", std::strerror(detail));
      case Kind::unexpectedExit:
        return fmt::format(
            "the subprocess exited {}, which is neither success, failure, nor "
            "the watchdog",
            detail);
    }
    return "unknown outcome";
  }
};

auto operator<<(std::ostream& os, SubprocessOutcome::Kind kind) -> std::ostream& {
  switch (kind) {
    case SubprocessOutcome::Kind::passed:
      return os << "passed";
    case SubprocessOutcome::Kind::taskFailed:
      return os << "taskFailed";
    case SubprocessOutcome::Kind::watchdogExpired:
      return os << "watchdogExpired";
    case SubprocessOutcome::Kind::killedBySignal:
      return os << "killedBySignal";
    case SubprocessOutcome::Kind::harnessError:
      return os << "harnessError";
    case SubprocessOutcome::Kind::unexpectedExit:
      return os << "unexpectedExit";
  }
  return os << "unknown";
}

auto run_in_subprocess(int (*task)()) -> SubprocessOutcome {
  const auto pid = fork();
  if (pid < 0) {
    return {SubprocessOutcome::Kind::harnessError, errno};
  }
  if (pid == 0) {
    // Without a handler SIGALRM terminates the child, so an expiry reaches the parent as a
    // signalled child and cannot be told apart from a crash. Exiting with a dedicated code
    // makes it an outcome we can name.
    struct sigaction action{};
    action.sa_handler = watchdogHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGALRM, &action, nullptr);
    alarm(kWatchdogSeconds);
    _exit(task());
  }

  int status = -1;
  if (waitpid(pid, &status, 0) != pid) {
    return {SubprocessOutcome::Kind::harnessError, errno};
  }
  if (WIFSIGNALED(status)) {
    return {SubprocessOutcome::Kind::killedBySignal, WTERMSIG(status)};
  }
  if (!WIFEXITED(status)) {
    return {SubprocessOutcome::Kind::unexpectedExit, -1};
  }
  const int code = WEXITSTATUS(status);
  if (code == 0) {
    return {SubprocessOutcome::Kind::passed, code};
  }
  if (code == kTaskFailed) {
    return {SubprocessOutcome::Kind::taskFailed, code};
  }
  if (code == kWatchdogExpired) {
    return {SubprocessOutcome::Kind::watchdogExpired, code};
  }
  return {SubprocessOutcome::Kind::unexpectedExit, code};
}

void require_subprocess_passes(const char* what, int (*task)()) {
  const auto outcome = run_in_subprocess(task);
  INFO(what << " -> " << outcome.describe());
  REQUIRE(outcome.kind == SubprocessOutcome::Kind::passed);
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
  require_subprocess_passes("reassignment", run_reassignment_regression);
}

TEST_CASE("SleeperThread can be joined before its owner releases resources",
          "[util][sleeper_thread]") {
  require_subprocess_passes("join before release", run_explicit_join_regression);
}

TEST_CASE("SleeperThread control flags are stable under concurrent wake and stop",
          "[util][sleeper_thread]") {
  require_subprocess_passes("concurrent wake and stop", run_control_flag_stress);
}

TEST_CASE("SleeperThread stopping itself does not terminate process", "[util][sleeper_thread]") {
  require_subprocess_passes("stop from own thread", run_stop_from_own_thread);
}

TEST_CASE("SleeperThread stopped while its worker logs from a catch block does not terminate",
          "[util][sleeper_thread]") {
  require_subprocess_passes("stop while logging", run_stop_while_logging_from_catch);
}
