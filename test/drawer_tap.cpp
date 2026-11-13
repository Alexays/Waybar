#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "util/drawer_tap.hpp"

using waybar::util::group_press;
using waybar::util::GroupPress;
using waybar::util::GroupPressState;

namespace {
GroupPressState hover_drawer() {
  return {.is_drawer = true,
          .click_to_reveal = false,
          .touch = false,
          .primary_press = true,
          .open = false,
          .on_drawer_child = false};
}
}  // namespace

TEST_CASE("a mouse click on a hover drawer is left to the module") {
  REQUIRE(group_press(hover_drawer()) == GroupPress::Pass);
}

TEST_CASE("a tap on a closed hover drawer opens it") {
  auto s = hover_drawer();
  s.touch = true;
  REQUIRE(group_press(s) == GroupPress::Open);
}

TEST_CASE("a tap on the leader of an open drawer closes it") {
  auto s = hover_drawer();
  s.touch = true;
  s.open = true;
  REQUIRE(group_press(s) == GroupPress::Close);
}

TEST_CASE("a tap on a module inside an open drawer reaches that module") {
  auto s = hover_drawer();
  s.touch = true;
  s.open = true;
  s.on_drawer_child = true;
  REQUIRE(group_press(s) == GroupPress::Pass);
}

TEST_CASE("a tap on a group that is not a drawer passes through") {
  auto s = hover_drawer();
  s.is_drawer = false;
  s.touch = true;
  REQUIRE(group_press(s) == GroupPress::Pass);
}

TEST_CASE("only a primary button press toggles") {
  auto s = hover_drawer();
  s.touch = true;
  s.primary_press = false;
  REQUIRE(group_press(s) == GroupPress::Pass);
}

TEST_CASE("click-to-reveal keeps toggling on mouse clicks") {
  auto s = hover_drawer();
  s.click_to_reveal = true;
  REQUIRE(group_press(s) == GroupPress::Open);
  s.open = true;
  REQUIRE(group_press(s) == GroupPress::Close);
}

TEST_CASE("click-to-reveal closes on a mouse click on a drawer child") {
  auto s = hover_drawer();
  s.click_to_reveal = true;
  s.open = true;
  s.on_drawer_child = true;
  REQUIRE(group_press(s) == GroupPress::Close);
}

TEST_CASE("click-to-reveal ignores other mouse buttons") {
  auto s = hover_drawer();
  s.click_to_reveal = true;
  s.primary_press = false;
  REQUIRE(group_press(s) == GroupPress::Pass);
}

TEST_CASE("a tap on a click-to-reveal group that is not a drawer toggles it") {
  auto s = hover_drawer();
  s.is_drawer = false;
  s.click_to_reveal = true;
  s.touch = true;
  REQUIRE(group_press(s) == GroupPress::Open);
}

TEST_CASE("a tap on a child of an open click-to-reveal drawer closes it like a click") {
  auto s = hover_drawer();
  s.click_to_reveal = true;
  s.touch = true;
  s.open = true;
  s.on_drawer_child = true;
  REQUIRE(group_press(s) == GroupPress::Close);
}
