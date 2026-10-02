#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <json/json.h>

#include <chrono>

#include "util/interval.hpp"

namespace {

using std::chrono::milliseconds;
using waybar::util::parseInterval;

Json::Value withInterval(const Json::Value& value) {
  Json::Value config;
  config["interval"] = value;
  return config;
}

}  // namespace

TEST_CASE("parseInterval reads a positive number as seconds", "[interval]") {
  REQUIRE(parseInterval(withInterval(5), 1) == milliseconds(5000));
}

TEST_CASE("parseInterval floors sub-millisecond values at 1ms", "[interval]") {
  REQUIRE(parseInterval(withInterval(0.0001), 1) == milliseconds(1));
}

TEST_CASE("parseInterval maps \"once\" to the maximum", "[interval]") {
  REQUIRE(parseInterval(withInterval("once"), 1) == milliseconds::max());
}

TEST_CASE("parseInterval falls back to the default when the key is absent", "[interval]") {
  REQUIRE(parseInterval(Json::Value{}, 3) == milliseconds(3000));
  REQUIRE(parseInterval(Json::Value{}, 0) == milliseconds(0));
}

TEST_CASE("parseInterval treats numeric 0 as a sentinel only for event-driven modules",
          "[interval]") {
  // A module whose default is 0 (e.g. custom) keeps 0 as the event-driven sentinel.
  REQUIRE(parseInterval(withInterval(0), 0) == milliseconds(0));
  // A periodic module (non-zero default) falls back to its default rather than busy-looping.
  REQUIRE(parseInterval(withInterval(0), 5) == milliseconds(5000));
}
