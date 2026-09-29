#include "util/backlight_brightness.hpp"

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <cmath>
#include <limits>
#include <vector>

static std::vector<int> scroll_writes(int current, int maximum, bool increase, double step = 1,
                                      double minimum = 0) {
  std::vector<int> writes;
  waybar::util::scroll_brightness(current, maximum, increase, step, minimum,
                                  [&](int target) { writes.push_back(target); });
  return writes;
}

TEST_CASE("Backlight scroll reaches zero below the displayed zero percent", "[backlight]") {
  REQUIRE(scroll_writes(389, 38787, false) == std::vector{1});
  REQUIRE(scroll_writes(1, 38787, false) == std::vector{0});
  REQUIRE(scroll_writes(193, 38787, false) == std::vector{0});
}

TEST_CASE("Backlight scroll respects fractional lower limits", "[backlight]") {
  // 2.5% of 38787 is 969.675: 970 is the lowest allowed device value.
  REQUIRE(scroll_writes(1000, 38787, false, 1, 2.5) == std::vector{970});
  REQUIRE(scroll_writes(970, 38787, false, 1, 2.5).empty());
  REQUIRE(scroll_writes(969, 38787, false, 1, 2.5).empty());
  REQUIRE(scroll_writes(0, 38787, true, 1, 2.5) == std::vector{388});
  REQUIRE(scroll_writes(100, 100, false, 100, 100).empty());
  REQUIRE(scroll_writes(2, 3, false, 100, 1) == std::vector{1});
}

TEST_CASE("Backlight scroll makes progress at device resolution", "[backlight]") {
  REQUIRE(scroll_writes(0, 1, true) == std::vector{1});
  REQUIRE(scroll_writes(1, 1, false) == std::vector{0});
  REQUIRE(scroll_writes(0, 38787, true, 0.1) == std::vector{39});
}

TEST_CASE("Backlight minimum preserves percentage boundaries", "[backlight]") {
  REQUIRE(scroll_writes(9, 10000, false, 1, 0.07) == std::vector{7});
  REQUIRE(scroll_writes(9, 10000, false, 1, std::nextafter(0.07, 1.0)) == std::vector{8});
  REQUIRE(scroll_writes(9, 10000, false, 1, std::nextafter(0.07, 0.0)) == std::vector{7});
}

TEST_CASE("Backlight step rounds half units upward at percentage boundaries", "[backlight]") {
  REQUIRE(scroll_writes(0, 10000, true, 0.285) == std::vector{29});
  REQUIRE(scroll_writes(0, 10000, true, std::nextafter(0.285, 0.0)) == std::vector{28});
  REQUIRE(scroll_writes(0, 10000, true, std::nextafter(0.285, 1.0)) == std::vector{29});
}

TEST_CASE("Backlight scroll writes the maximum even when the cache is already there",
          "[backlight]") {
  REQUIRE(scroll_writes(38787, 38787, true) == std::vector{38787});
}

TEST_CASE("Backlight scroll bounds arithmetic before integer conversion", "[backlight]") {
  constexpr int maximum = std::numeric_limits<int>::max();
  constexpr double huge = std::numeric_limits<double>::max();
  REQUIRE(scroll_writes(1, maximum, true, huge, 0) == std::vector{maximum});
  REQUIRE(scroll_writes(maximum, maximum, false, huge, 0) == std::vector{0});
  REQUIRE(scroll_writes(maximum - 1, maximum, true) == std::vector{maximum});
  REQUIRE(scroll_writes(1, maximum, false, 1, -huge) == std::vector{0});
  REQUIRE(scroll_writes(1, maximum, false, 1, huge).empty());
  REQUIRE(scroll_writes(0, 3, true, std::numeric_limits<double>::denorm_min(), 0) ==
          std::vector{1});
  REQUIRE(scroll_writes(2, 3, false, 100, std::numeric_limits<double>::denorm_min()) ==
          std::vector{1});
}

TEST_CASE("Backlight scroll ignores invalid steps and unavailable ranges", "[backlight]") {
  constexpr double infinity = std::numeric_limits<double>::infinity();
  constexpr double nan = std::numeric_limits<double>::quiet_NaN();
  for (bool increase : {false, true}) {
    for (double step : {0.0, -1.0, infinity, -infinity, nan}) {
      REQUIRE(scroll_writes(100, 38787, increase, step, 0).empty());
    }
    for (double minimum : {infinity, -infinity, nan}) {
      REQUIRE(scroll_writes(100, 38787, increase, 1, minimum).empty());
    }
    REQUIRE(scroll_writes(0, 0, increase).empty());
    REQUIRE(scroll_writes(0, -1, increase).empty());
  }
}

TEST_CASE("Backlight scroll remains bounded and moves toward each limit", "[backlight]") {
  for (int maximum : {3, 255, 38787}) {
    for (int current : {0, 1, maximum - 1, maximum}) {
      for (double step : {0.001, 1.0, 100.0}) {
        const auto up = scroll_writes(current, maximum, true, step);
        REQUIRE(up.size() == 1);
        REQUIRE(up[0] <= maximum);
        REQUIRE((current == maximum ? up[0] == current : up[0] > current));
        const auto down = scroll_writes(current, maximum, false, step);
        if (current == 0) {
          REQUIRE(down.empty());
        } else {
          REQUIRE(down.size() == 1);
          REQUIRE(down[0] >= 0);
          REQUIRE(down[0] < current);
        }
      }
    }
  }
}
