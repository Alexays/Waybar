#include "util/waybar_time.hpp"

#include <date/date.h>
#include <date/tz.h>

#include <catch2/catch_all.hpp>
#include <chrono>
#include <stdexcept>

using namespace std::literals::chrono_literals;

/*
 * Check that the date/time formatter with locale and timezone support is working as expected.
 */

const date::zoned_time<std::chrono::seconds> TEST_TIME = date::make_zoned(
    "UTC", date::local_days{date::Monday[1] / date::January / 2022} + 13h + 4min + 5s);

TEST_CASE("Format UTC time", "[clock][util]") {
  waybar::waybar_time tm{std::locale("C"), TEST_TIME};

  REQUIRE(fmt::format("{}", tm).empty());  // no format specified
  REQUIRE(fmt::format("{:%c %Z}", tm) == "Mon Jan  3 13:04:05 2022 UTC");
  REQUIRE(fmt::format("{arg:%Y%m%d%H%M%S}", fmt::arg("arg", tm)) == "20220103130405");

  /* Test a few locales that are most likely to be present */
  SECTION("US locale") {
    try {
      tm.locale = std::locale("en_US");

      REQUIRE(fmt::format("{}", tm).empty());  // no format specified
      REQUIRE_THAT(fmt::format("{:%c}", tm),   // HowardHinnant/date#704
                   Catch::Matchers::StartsWith("Mon 03 Jan 2022 01:04:05 PM"));
      REQUIRE(fmt::format("{:%x %X}", tm) == "01/03/2022 01:04:05 PM");
      REQUIRE(fmt::format("{arg:%Y%m%d%H%M%S}", fmt::arg("arg", tm)) == "20220103130405");
    } catch (const std::runtime_error&) {
      // locale not found; ignore
    }
  }
  SECTION("GB locale") {
    try {
      tm.locale = std::locale("en_GB");

      REQUIRE(fmt::format("{}", tm).empty());  // no format specified
      REQUIRE_THAT(fmt::format("{:%c}", tm),   // HowardHinnant/date#704
                   Catch::Matchers::StartsWith("Mon 03 Jan 2022 13:04:05"));
      REQUIRE(fmt::format("{:%x %X}", tm) == "03/01/22 13:04:05");
      REQUIRE(fmt::format("{arg:%Y%m%d%H%M%S}", fmt::arg("arg", tm)) == "20220103130405");
    } catch (const std::runtime_error&) {
      // locale not found; ignore
    }
  }
}

TEST_CASE("Format zoned time", "[clock][util]") {
  waybar::waybar_time tm{std::locale("C"), date::make_zoned("America/New_York", TEST_TIME)};

  REQUIRE(fmt::format("{}", tm).empty());  // no format specified
  REQUIRE(fmt::format("{:%c %Z}", tm) == "Mon Jan  3 08:04:05 2022 EST");
  REQUIRE(fmt::format("{arg:%Y%m%d%H%M%S}", fmt::arg("arg", tm)) == "20220103080405");

  /* Test a few locales that are most likely to be present */
  SECTION("US locale") {
    try {
      tm.locale = std::locale("en_US");

      REQUIRE(fmt::format("{}", tm).empty());  // no format specified
      REQUIRE_THAT(fmt::format("{:%c}", tm),   // HowardHinnant/date#704
                   Catch::Matchers::StartsWith("Mon 03 Jan 2022 08:04:05 AM"));
      REQUIRE(fmt::format("{:%x %X}", tm) == "01/03/2022 08:04:05 AM");
      REQUIRE(fmt::format("{arg:%Y%m%d%H%M%S}", fmt::arg("arg", tm)) == "20220103080405");
    } catch (const std::runtime_error&) {
      // locale not found; ignore
    }
  }

  SECTION("GB locale") {
    try {
      tm.locale = std::locale("en_GB");

      REQUIRE(fmt::format("{}", tm).empty());  // no format specified
      REQUIRE_THAT(fmt::format("{:%c}", tm),   // HowardHinnant/date#704
                   Catch::Matchers::StartsWith("Mon 03 Jan 2022 08:04:05"));
      REQUIRE(fmt::format("{:%x %X}", tm) == "03/01/22 08:04:05");
      REQUIRE(fmt::format("{arg:%Y%m%d%H%M%S}", fmt::arg("arg", tm)) == "20220103080405");
    } catch (const std::runtime_error&) {
      // locale not found; ignore
    }
  }
}
