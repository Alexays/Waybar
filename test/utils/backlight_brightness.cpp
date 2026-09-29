#include "util/backlight_brightness.hpp"

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <limits>

using waybar::util::brightness_after_scroll;

TEST_CASE("Backlight scroll reaches zero below the displayed zero percent", "[backlight]") {
  for (int current = 1; current <= 193; ++current) {
    REQUIRE(brightness_after_scroll(current, 38787, false, 1, 0) == 0);
  }
  REQUIRE(brightness_after_scroll(1941, 38787, false, 1, 0) == 1553);
  REQUIRE(brightness_after_scroll(389, 38787, false, 1, 0) == 1);
  REQUIRE(brightness_after_scroll(1, 38787, false, 1, 0) == 0);
}

TEST_CASE("Backlight scroll respects fractional lower limits", "[backlight]") {
  // 2.5% of 38787 is 969.675: 970 is the lowest allowed device value.
  REQUIRE(brightness_after_scroll(1000, 38787, false, 1, 2.5) == 970);
  REQUIRE(brightness_after_scroll(970, 38787, false, 1, 2.5) == 970);
  REQUIRE(brightness_after_scroll(969, 38787, false, 1, 2.5) == 969);
  REQUIRE(brightness_after_scroll(0, 38787, true, 1, 2.5) == 388);
  REQUIRE(brightness_after_scroll(100, 38787, false, 1, -1) == 0);
  REQUIRE(brightness_after_scroll(100, 38787, false, 1, 101) == 100);
  REQUIRE(brightness_after_scroll(100, 100, false, 100, 100) == 100);
}

TEST_CASE("Backlight scroll makes progress at device resolution", "[backlight]") {
  REQUIRE(brightness_after_scroll(0, 3, true, 1, 0) == 1);
  REQUIRE(brightness_after_scroll(1, 3, false, 1, 0) == 0);
  REQUIRE(brightness_after_scroll(2, 3, false, 100, 1) == 1);
  REQUIRE(brightness_after_scroll(0, 38787, true, 0.1, 0) == 39);
  REQUIRE(brightness_after_scroll(0, 38787, true, 0.0001, 0) == 1);
  REQUIRE(brightness_after_scroll(38786, 38787, true, 1, 0) == 38787);
  REQUIRE(brightness_after_scroll(38787, 38787, true, 1, 0) == 38787);
  REQUIRE(brightness_after_scroll(0, 38787, false, 1, 0) == 0);
}

TEST_CASE("Backlight scroll bounds arithmetic before integer conversion", "[backlight]") {
  constexpr int maximum = std::numeric_limits<int>::max();
  constexpr double huge = std::numeric_limits<double>::max();
  REQUIRE(brightness_after_scroll(1, maximum, true, huge, 0) == maximum);
  REQUIRE(brightness_after_scroll(maximum, maximum, false, huge, 0) == 0);
  REQUIRE(brightness_after_scroll(maximum - 1, maximum, true, 1, 0) == maximum);
  REQUIRE(brightness_after_scroll(1, maximum, false, 1, -huge) == 0);
  REQUIRE(brightness_after_scroll(1, maximum, false, 1, huge) == 1);
  REQUIRE(brightness_after_scroll(0, 3, true, std::numeric_limits<double>::denorm_min(), 0) == 1);
  REQUIRE(brightness_after_scroll(2, 3, false, 100, std::numeric_limits<double>::denorm_min()) ==
          1);
}

TEST_CASE("Backlight scroll ignores invalid steps and unavailable ranges", "[backlight]") {
  constexpr double infinity = std::numeric_limits<double>::infinity();
  constexpr double nan = std::numeric_limits<double>::quiet_NaN();
  for (bool increase : {false, true}) {
    for (double step : {0.0, -1.0, infinity, -infinity, nan}) {
      REQUIRE(brightness_after_scroll(100, 38787, increase, step, 0) == 100);
    }
    for (double minimum : {infinity, -infinity, nan}) {
      REQUIRE(brightness_after_scroll(100, 38787, increase, 1, minimum) == 100);
    }
    REQUIRE(brightness_after_scroll(0, 0, increase, 1, 0) == 0);
    REQUIRE(brightness_after_scroll(0, -1, increase, 1, 0) == 0);
  }
}

TEST_CASE("Backlight scroll is bounded and moves toward each limit", "[backlight]") {
  for (int maximum : {1, 3, 99, 100, 255, 38787}) {
    for (double step : {0.001, 0.1, 1.0, 5.0, 100.0, 1e300}) {
      for (int current = 0; current <= maximum; ++current) {
        const int up = brightness_after_scroll(current, maximum, true, step, 0);
        const int down = brightness_after_scroll(current, maximum, false, step, 0);
        REQUIRE(up <= maximum);
        REQUIRE(down >= 0);
        REQUIRE((current == maximum ? up == current : up > current));
        REQUIRE((current == 0 ? down == current : down < current));
      }
    }
  }
}
