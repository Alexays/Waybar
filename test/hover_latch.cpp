#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <string>

#include "util/hover_latch.hpp"

using waybar::util::HoverLatch;

TEST_CASE("repeated crossings run each handler once per hover") {
  std::string log;
  HoverLatch latch{[&log](bool entering) { log += entering ? 'E' : 'L'; }};

  latch.set(true);
  latch.set(true);
  latch.set(false);
  latch.set(false);
  latch.set(true);

  REQUIRE(log == "ELE");
}
