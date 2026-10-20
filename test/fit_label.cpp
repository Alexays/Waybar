#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "util/fit_label.hpp"

using waybar::util::fit_scale;

TEST_CASE("text that fits keeps its normal size") { REQUIRE(fit_scale(1.0, 20, 32) == 1.0); }

TEST_CASE("text that is too tall scales down to the available height") {
  // Two 18px lines in a 27px slot.
  REQUIRE(fit_scale(1.0, 36, 27) == 0.75);
}

TEST_CASE("scaled text grows back when there is room, but not above normal size") {
  REQUIRE(fit_scale(0.5, 18, 27) == 0.75);
  REQUIRE(fit_scale(0.5, 18, 100) == 1.0);
}

TEST_CASE("empty text resets the scale and a missing allocation keeps it") {
  REQUIRE(fit_scale(0.5, 0, 27) == 1.0);
  REQUIRE(fit_scale(0.5, 18, 0) == 0.5);
}
