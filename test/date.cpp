#include "util/date.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#else
#include <catch2/catch.hpp>
#endif

#ifndef SKIP
#define SKIP(...)    \
  WARN(__VA_ARGS__); \
  return
#endif

using namespace std::literals::chrono_literals;

/*
 * Check that the date/time formatter with locale and timezone support is working as expected.
 */

const date::zoned_time<std::chrono::seconds> TEST_TIME = date::zoned_time{
    "UTC", date::local_days{date::Monday[1] / date::January / 2022} + 13h + 4min + 5s};

/*
 * Check if the date formatted with LC_TIME=en_US is within expectations.
 *
 * The check expects Glibc output style and will fail with FreeBSD (different implementation)
 * or musl (no implementation).
 */
static const bool LC_TIME_is_sane = []() {
  try {
    std::stringstream ss;
    ss.imbue(std::locale("en_US.UTF-8"));

    time_t t = 1641211200;
    std::tm tm = *std::gmtime(&t);

    ss << std::put_time(&tm, "%x %X");
    return ss.str() == "01/03/2022 12:00:00 PM";
  } catch (std::exception &) {
    return false;
  }
}();

TEST_CASE("Format UTC time", "[clock][util]") {
  const auto loc = std::locale("C");
  const auto tm = TEST_TIME;

  CHECK(fmt::format(loc, "{}", tm).empty());  // no format specified
  CHECK(fmt::format(loc, "{:%c %Z}", tm) == "Mon Jan  3 13:04:05 2022 UTC");
  CHECK(fmt::format(loc, "{arg:%Y%m%d%H%M%S}", fmt::arg("arg", tm)) == "20220103130405");

  if (!LC_TIME_is_sane) {
    SKIP("Locale support check failed, skip tests");
  }

  /* Test a few locales that are most likely to be present */
  SECTION("US locale") {
    try {
      const auto loc = std::locale("en_US.UTF-8");

      CHECK(fmt::format(loc, "{}", tm).empty());  // no format specified
      CHECK_THAT(fmt::format(loc, "{:%c}", tm),   // HowardHinnant/date#704
                 Catch::Matchers::StartsWith("Mon 03 Jan 2022 01:04:05 PM"));
      CHECK(fmt::format(loc, "{:%x %X}", tm) == "01/03/2022 01:04:05 PM");
      CHECK(fmt::format(loc, "{arg:%Y%m%d%H%M%S}", fmt::arg("arg", tm)) == "20220103130405");
    } catch (const std::runtime_error &) {
      WARN("Locale en_US not found, skip tests");
    }
  }
  SECTION("GB locale") {
    try {
      const auto loc = std::locale("en_GB.UTF-8");

      CHECK(fmt::format(loc, "{}", tm).empty());  // no format specified
      CHECK_THAT(fmt::format(loc, "{:%c}", tm),   // HowardHinnant/date#704
                 Catch::Matchers::StartsWith("Mon 03 Jan 2022 13:04:05"));
      CHECK(fmt::format(loc, "{:%x %X}", tm) == "03/01/22 13:04:05");
      CHECK(fmt::format(loc, "{arg:%Y%m%d%H%M%S}", fmt::arg("arg", tm)) == "20220103130405");
    } catch (const std::runtime_error &) {
      WARN("Locale en_GB not found, skip tests");
    }
  }
  SECTION("Global locale") {
    try {
      const auto loc = std::locale::global(std::locale("en_US.UTF-8"));

      CHECK(fmt::format("{}", tm).empty());  // no format specified
      CHECK_THAT(fmt::format("{:%c}", tm),   // HowardHinnant/date#704
                 Catch::Matchers::StartsWith("Mon 03 Jan 2022 01:04:05 PM"));
      CHECK(fmt::format("{:%x %X}", tm) == "01/03/2022 01:04:05 PM");
      CHECK(fmt::format("{arg:%Y%m%d%H%M%S}", fmt::arg("arg", tm)) == "20220103130405");

      std::locale::global(loc);
    } catch (const std::runtime_error &) {
      WARN("Locale en_US not found, skip tests");
    }
  }
}

TEST_CASE("Format zoned time", "[clock][util]") {
  const auto loc = std::locale("C");
  const auto tm = date::zoned_time{"America/New_York", TEST_TIME};

  CHECK(fmt::format(loc, "{}", tm).empty());  // no format specified
  CHECK(fmt::format(loc, "{:%c %Z}", tm) == "Mon Jan  3 08:04:05 2022 EST");
  CHECK(fmt::format(loc, "{arg:%Y%m%d%H%M%S}", fmt::arg("arg", tm)) == "20220103080405");

  if (!LC_TIME_is_sane) {
    SKIP("Locale support check failed, skip tests");
  }

  /* Test a few locales that are most likely to be present */
  SECTION("US locale") {
    try {
      const auto loc = std::locale("en_US.UTF-8");

      CHECK(fmt::format(loc, "{}", tm).empty());  // no format specified
      CHECK_THAT(fmt::format(loc, "{:%c}", tm),   // HowardHinnant/date#704
                 Catch::Matchers::StartsWith("Mon 03 Jan 2022 08:04:05 AM"));
      CHECK(fmt::format(loc, "{:%x %X}", tm) == "01/03/2022 08:04:05 AM");
      CHECK(fmt::format(loc, "{arg:%Y%m%d%H%M%S}", fmt::arg("arg", tm)) == "20220103080405");
    } catch (const std::runtime_error &) {
      WARN("Locale en_US not found, skip tests");
    }
  }
  SECTION("GB locale") {
    try {
      const auto loc = std::locale("en_GB.UTF-8");

      CHECK(fmt::format(loc, "{}", tm).empty());  // no format specified
      CHECK_THAT(fmt::format(loc, "{:%c}", tm),   // HowardHinnant/date#704
                 Catch::Matchers::StartsWith("Mon 03 Jan 2022 08:04:05"));
      CHECK(fmt::format(loc, "{:%x %X}", tm) == "03/01/22 08:04:05");
      CHECK(fmt::format(loc, "{arg:%Y%m%d%H%M%S}", fmt::arg("arg", tm)) == "20220103080405");
    } catch (const std::runtime_error &) {
      WARN("Locale en_GB not found, skip tests");
    }
  }
  SECTION("Global locale") {
    try {
      const auto loc = std::locale::global(std::locale("en_US.UTF-8"));

      CHECK(fmt::format("{}", tm).empty());  // no format specified
      CHECK_THAT(fmt::format("{:%c}", tm),   // HowardHinnant/date#704
                 Catch::Matchers::StartsWith("Mon 03 Jan 2022 08:04:05 AM"));
      CHECK(fmt::format("{:%x %X}", tm) == "01/03/2022 08:04:05 AM");
      CHECK(fmt::format("{arg:%Y%m%d%H%M%S}", fmt::arg("arg", tm)) == "20220103080405");

      std::locale::global(loc);
    } catch (const std::runtime_error &) {
      WARN("Locale en_US not found, skip tests");
    }
  }
}
