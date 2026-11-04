#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <glibmm/main.h>

#include <chrono>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "util/command.hpp"
#include "util/command_line_stream.hpp"

namespace {

struct StreamResult {
  std::vector<std::string> lines;
  std::optional<int> exit_code;
  bool timed_out = false;
};

auto run_stream_command(const std::string& cmd) -> StreamResult {
  StreamResult result;
  auto loop = Glib::MainLoop::create();

  auto timeout = Glib::signal_timeout().connect(
      [&]() {
        result.timed_out = true;
        loop->quit();
        return false;
      },
      3000);

  waybar::util::command::LineStream stream(
      "", [&](const std::string& line) { result.lines.push_back(line); },
      [&](int exit_code) {
        result.exit_code = exit_code;
        loop->quit();
      });

  stream.start(cmd);
  loop->run();
  timeout.disconnect();
  return result;
}

}  // namespace

TEST_CASE("command::LineStream emits complete lines and flushes trailing output",
          "[util][command_line_stream]") {
  const auto result = run_stream_command("printf 'first\\nsecond'");

  REQUIRE_FALSE(result.timed_out);
  REQUIRE(result.exit_code.has_value());
  REQUIRE(*result.exit_code == 0);
  REQUIRE(result.lines == std::vector<std::string>{"first", "second"});
}

TEST_CASE("command::LineStream does not emit an extra line after newline-terminated output",
          "[util][command_line_stream]") {
  const auto result = run_stream_command("printf 'first\\nsecond\\n'");

  REQUIRE_FALSE(result.timed_out);
  REQUIRE(result.exit_code.has_value());
  REQUIRE(*result.exit_code == 0);
  REQUIRE(result.lines == std::vector<std::string>{"first", "second"});
}

TEST_CASE("command::LineStream bounds what it keeps of a line", "[util][command_line_stream]") {
  using waybar::util::command::kMaxOutputSize;

  const auto result =
      run_stream_command("echo first; head -c 1000000 /dev/zero | tr '\\0' A; echo; echo last");

  REQUIRE_FALSE(result.timed_out);
  REQUIRE(result.exit_code.has_value());
  REQUIRE(*result.exit_code == 0);
  REQUIRE(result.lines ==
          std::vector<std::string>{"first", std::string(kMaxOutputSize, 'A'), "last"});
}

namespace {

// Starts the command, waits for its first line, leaves it unread for a while and stops it.
// A regression here does not fail, it hangs in stop().
auto stopStreamCommand(const std::string& cmd) -> bool {
  auto loop = Glib::MainLoop::create();
  waybar::util::command::LineStream stream(
      "", [&](const std::string&) { loop->quit(); }, [](int) {});

  stream.start(cmd);
  loop->run();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  stream.stop();
  return !stream.running();
}

}  // namespace

TEST_CASE("command::LineStream::stop ends a command blocked writing to the full pipe",
          "[util][command_line_stream]") {
  // The line exceeds the pipe size, so the shell is still in printf when SIGTERM arrives
  REQUIRE(stopStreamCommand(
      "trap 'exit 0' TERM; echo ready; while :; do printf '%0200000d\\n' 0; done"));
}

TEST_CASE("command::LineStream::stop ends a command that ignores SIGTERM",
          "[util][command_line_stream]") {
  REQUIRE(stopStreamCommand("trap '' TERM; echo ready; while :; do sleep 1; done"));
}
