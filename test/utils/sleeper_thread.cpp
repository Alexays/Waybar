#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <chrono>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#include "util/sleeper_thread.hpp"

namespace waybar::util {
SafeSignal<bool>& prepare_for_sleep() {
  static SafeSignal<bool> signal;
  return signal;
}
}  // namespace waybar::util

namespace {
int run_reassignment_regression() {
  waybar::util::SleeperThread thread;
  thread = [] { std::this_thread::sleep_for(std::chrono::milliseconds(10)); };
  thread = [] { std::this_thread::sleep_for(std::chrono::milliseconds(1)); };
  return 0;
}
}  // namespace

TEST_CASE("SleeperThread reassignment does not terminate process", "[util][sleeper_thread]") {
  const auto pid = fork();
  REQUIRE(pid >= 0);

  if (pid == 0) {
    _exit(run_reassignment_regression());
  }

  int status = -1;
  REQUIRE(waitpid(pid, &status, 0) == pid);
  REQUIRE(WIFEXITED(status));
  REQUIRE(WEXITSTATUS(status) == 0);
}
