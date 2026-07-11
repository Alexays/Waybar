#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <glibmm/main.h>

#include <optional>
#include <string>
#include <vector>

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
