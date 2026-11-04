#include "util/css_class.hpp"

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

using waybar::util::is_bar_state_class;

TEST_CASE("is_bar_state_class", "[util]") {
  SECTION("names the bar's own state") {
    for (const char* name :
         {"hidden", "solo", "top", "mode-default", "battery-charging", "toplevel-active"}) {
      REQUIRE(is_bar_state_class(name));
    }
  }

  SECTION("leaves ordinary app_ids alone") {
    for (const char* name : {"chromium", "org.gnome.Nautilus", "hidden-files", "mode", ""}) {
      REQUIRE_FALSE(is_bar_state_class(name));
    }
  }
}
