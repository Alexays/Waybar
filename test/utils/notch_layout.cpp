#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "util/notch_layout.hpp"

using waybar::util::notch_layout;

// A 1000 px wide area with a 100 px zone in the middle: [450, 550).
constexpr int LEFT = 0;
constexpr int RIGHT = 1000;
constexpr int ZONE_START = 450;
constexpr int ZONE_END = 550;

TEST_CASE("items that fit right of the zone are packed against the right edge") {
  auto layout = notch_layout({100, 200}, LEFT, RIGHT, ZONE_START, ZONE_END, 0);
  REQUIRE(layout.slots[1].visible);
  REQUIRE(layout.slots[1].x == 800);
  REQUIRE(layout.slots[0].x == 700);
  REQUIRE(layout.segments == std::vector<std::pair<int, int>>{{700, 1000}});
}

TEST_CASE("an item that would touch the zone hops to its left side") {
  // 300 + 100 fill [600, 1000); the next 100 would cover [500, 600) and touch the zone.
  auto layout = notch_layout({100, 100, 300}, LEFT, RIGHT, ZONE_START, ZONE_END, 0);
  REQUIRE(layout.slots[2].x == 700);
  REQUIRE(layout.slots[1].x == 600);
  REQUIRE(layout.slots[0].x == 350);
  REQUIRE(layout.segments == std::vector<std::pair<int, int>>{{600, 1000}, {350, 450}});
}

TEST_CASE("an item that ends exactly at the zone edge does not hop") {
  auto layout = notch_layout({450}, LEFT, RIGHT, ZONE_START, ZONE_END, 0);
  REQUIRE(layout.slots[0].x == 550);
}

TEST_CASE("spacing is kept between items but not across the zone") {
  auto layout = notch_layout({100, 400}, LEFT, RIGHT, ZONE_START, ZONE_END, 10);
  REQUIRE(layout.slots[1].x == 600);
  // 600 - 10 - 100 = 490 would touch the zone, so it goes right up to the zone's left edge.
  REQUIRE(layout.slots[0].x == 350);
}

TEST_CASE("items that do not fit are hidden, together with everything before them") {
  // 450 fills [550, 1000), 400 hops to [50, 450), and the next 100 no longer fits.
  auto layout = notch_layout({10, 100, 400, 450}, LEFT, RIGHT, ZONE_START, ZONE_END, 0);
  REQUIRE(layout.slots[3].visible);
  REQUIRE(layout.slots[2].visible);
  REQUIRE_FALSE(layout.slots[1].visible);
  REQUIRE_FALSE(layout.slots[0].visible);  // would fit, but must not jump ahead of [1]
}

TEST_CASE("the left limit counts, e.g. modules-left ending at 300") {
  auto layout = notch_layout({200, 450}, 300, RIGHT, ZONE_START, ZONE_END, 0);
  REQUIRE(layout.slots[1].visible);
  REQUIRE_FALSE(layout.slots[0].visible);  // [250, 450) crosses 300
}

TEST_CASE("without a zone it behaves like a right-aligned box") {
  auto layout = notch_layout({100, 450, 100}, LEFT, RIGHT, 0, 0, 0);
  REQUIRE(layout.slots[0].x == 350);
  REQUIRE(layout.segments == std::vector<std::pair<int, int>>{{350, 1000}});
}

TEST_CASE("no items, no segments") {
  auto layout = notch_layout({}, LEFT, RIGHT, ZONE_START, ZONE_END, 0);
  REQUIRE(layout.slots.empty());
  REQUIRE(layout.segments.empty());
}
