#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <glibmm/main.h>

#include <string>

#include "util/hover_latch.hpp"

using waybar::util::HoverLatch;

namespace {

void spin(int ms) {
  auto loop = Glib::MainLoop::create();
  Glib::signal_timeout().connect_once([&loop] { loop->quit(); }, ms);
  loop->run();
}

}  // namespace

TEST_CASE("repeated crossings run each handler once per hover") {
  std::string log;
  HoverLatch latch{0, [&log](bool entering) { log += entering ? 'E' : 'L'; }};

  latch.set(true);
  latch.set(true);
  latch.set(false);
  latch.set(false);
  latch.set(true);

  REQUIRE(log == "ELE");
}

TEST_CASE("a delayed enter waits for the delay") {
  std::string log;
  HoverLatch latch{20, [&log](bool entering) { log += entering ? 'E' : 'L'; }};

  latch.set(true);
  REQUIRE(log.empty());
  spin(60);
  REQUIRE(log == "E");
  latch.set(false);
  REQUIRE(log == "EL");
}

TEST_CASE("a hover shorter than the delay runs neither handler") {
  std::string log;
  HoverLatch latch{20, [&log](bool entering) { log += entering ? 'E' : 'L'; }};

  latch.set(true);
  latch.set(false);
  spin(60);

  REQUIRE(log.empty());
}

TEST_CASE("reset drops the hover without running leave") {
  std::string log;
  HoverLatch latch{20, [&log](bool entering) { log += entering ? 'E' : 'L'; }};

  latch.set(true);
  latch.reset();
  spin(60);
  REQUIRE(log.empty());

  latch.set(true);
  spin(60);
  REQUIRE(log == "E");
}
