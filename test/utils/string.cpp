#include "util/string.hpp"

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

// The layout half of a hyprland "activelayout" payload, split the way
// Language::onEvent splits it.
static std::string layoutOf(const std::string& payload) {
  const auto sep = rfindOutsideParens(payload, ',');
  if (sep == std::string::npos) return "";
  return payload.substr(sep + 1);
}

TEST_CASE("rfindOutsideParens", "[string]") {
  SECTION("returns npos when the character is absent") {
    REQUIRE(rfindOutsideParens("abc", ',') == std::string::npos);
    REQUIRE(rfindOutsideParens("", ',') == std::string::npos);
  }

  SECTION("finds the last match when no parentheses are present") {
    REQUIRE(rfindOutsideParens("a,b,c", ',') == 3);
  }

  SECTION("ignores matches nested inside parentheses") {
    REQUIRE(rfindOutsideParens("a,b(c,d)", ',') == 1);
    REQUIRE(rfindOutsideParens("a(b,c)d(e,f)", ',') == std::string::npos);
  }

  SECTION("handles nested parentheses") { REQUIRE(rfindOutsideParens("a,b(c(d,e),f)", ',') == 1); }

  SECTION("tolerates unbalanced parentheses") {
    // A stray '(' left of the match must not shield it.
    REQUIRE(rfindOutsideParens("a(b,c", ',') == 3);
    // A stray ')' must not drive the depth negative and swallow later matches.
    REQUIRE(rfindOutsideParens("a)b,c", ',') == 3);
  }
}

// Regression test for https://github.com/Alexays/Waybar/issues/4586
TEST_CASE("hyprland activelayout payload split", "[string]") {
  SECTION("keyboard name containing brackets, layout without") {
    // Splitting on the last '(' picked the keyboard's own bracket, leaving no
    // comma behind it, so the event was discarded and the layout never updated.
    REQUIRE(layoutOf("ite-tech.-inc.-ite-device(8910)-keyboard,Russian") == "Russian");
  }

  SECTION("layout containing commas inside its variant parentheses") {
    REQUIRE(layoutOf("micro-star-int'l-co.,-ltd.-msi-gk50-elite-gaming-keyboard,"
                     "English (US, intl., with dead keys)") ==
            "English (US, intl., with dead keys)");
  }

  SECTION("brackets in the keyboard name and commas in the layout variant") {
    REQUIRE(layoutOf("ite-tech.-inc.-ite-device(8910)-keyboard,English (US, intl.)") ==
            "English (US, intl.)");
  }

  SECTION("plain keyboard name and layout") {
    REQUIRE(layoutOf("at-translated-set-2-keyboard,English (US)") == "English (US)");
  }
}
