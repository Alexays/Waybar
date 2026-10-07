#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "util/center_layout.hpp"

using waybar::util::center_layout;

TEST_CASE("center block stays in the middle while both sides fit") {
  auto layout = center_layout(1000, 0, 100, 0, 200, 0, 300);

  REQUIRE(layout.start_size == 100);
  REQUIRE(layout.center_pos == 400);
  REQUIRE(layout.center_size == 200);
  REQUIRE(layout.end_size == 300);
}

TEST_CASE("wide end side pushes the center block toward the start") {
  // GtkBox would cap the end side at (1000 - 200) / 2 = 400 and ellipsize it.
  auto layout = center_layout(1000, 0, 100, 0, 200, 0, 600);

  REQUIRE(layout.end_size == 600);
  REQUIRE(layout.center_pos == 200);
  REQUIRE(layout.start_size == 100);
}

TEST_CASE("wide start side pushes the center block toward the end") {
  auto layout = center_layout(1000, 0, 600, 0, 200, 0, 100);

  REQUIRE(layout.start_size == 600);
  REQUIRE(layout.center_pos == 600);
  REQUIRE(layout.end_size == 100);
}

TEST_CASE("both sides overflowing split the space evenly") {
  auto layout = center_layout(1000, 50, 600, 0, 200, 50, 700);

  REQUIRE(layout.start_size == 400);
  REQUIRE(layout.center_pos == 400);
  REQUIRE(layout.end_size == 400);
}

TEST_CASE("overflowing start side yields to the end side's minimum") {
  auto layout = center_layout(1000, 0, 600, 0, 200, 500, 700);

  REQUIRE(layout.end_size == 500);
  REQUIRE(layout.start_size == 300);
  REQUIRE(layout.center_pos == 300);
}
