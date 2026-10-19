#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <optional>
#include <regex>

#include "util/slider_value.hpp"

namespace {

using waybar::util::parseSliderValue;

std::optional<double> parse(const std::string& raw, int min = 0, int max = 100) {
  return parseSliderValue(raw, min, max, std::nullopt);
}

}  // namespace

TEST_CASE("parseSliderValue reads plain and fractional numbers", "[slider_value]") {
  REQUIRE(parse("58") == 58.0);
  REQUIRE(parse("100.000000") == 100.0);  // hyprctl hyprsunset gamma
}

TEST_CASE("parseSliderValue strips a trailing percent and whitespace", "[slider_value]") {
  REQUIRE(parse("42%") == 42.0);
  REQUIRE(parse("  73  ") == 73.0);
}

TEST_CASE("parseSliderValue uses only the first line", "[slider_value]") {
  REQUIRE(parse("75\nignored") == 75.0);
}

TEST_CASE("parseSliderValue clamps to [min, max]", "[slider_value]") {
  REQUIRE(parse("150", 10, 100) == 100.0);
  REQUIRE(parse("5", 10, 100) == 10.0);
  REQUIRE(parse("250", 0, 200) == 200.0);  // gamma_max: 200 must not cap at 100
}

TEST_CASE("parseSliderValue returns nullopt instead of 0 on failure", "[slider_value]") {
  REQUIRE(parse("") == std::nullopt);
  REQUIRE(parse("   ") == std::nullopt);
  REQUIRE(parse("garbage") == std::nullopt);
  REQUIRE(parse("nan") == std::nullopt);  // std::stod would accept nan; we reject it
  REQUIRE(parse("inf") == std::nullopt);
}

TEST_CASE("parseSliderValue with value-regex reads capture group 1", "[slider_value]") {
  std::regex re(R"(([0-9]+(\.[0-9]+)?)%?$)");
  REQUIRE(parseSliderValue("brightness: 80%", 0, 100, re) == 80.0);
  REQUIRE(parseSliderValue("level 33.5", 0, 100, re) == 33.5);
}

TEST_CASE("parseSliderValue with value-regex stays stale when nothing matches", "[slider_value]") {
  std::regex re(R"(([0-9]+)%$)");  // requires a trailing percent
  // The heuristic alone would read 90 here; the configured regex must win.
  REQUIRE(parseSliderValue("90 units", 0, 100, re) == std::nullopt);
}
