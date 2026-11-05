#include "util/utf8_string.hpp"

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <string>

using waybar::util::utf8_truncate;
using waybar::util::utf8_width;

static std::string repeat(const std::string& s, size_t n) {
  std::string result;
  for (size_t i = 0; i < n; ++i) result += s;
  return result;
}

static std::string truncated(std::string s, size_t max_len) {
  utf8_truncate(s, ".", max_len);
  return s;
}

TEST_CASE("utf8_width", "[utf8_string]") {
  // flag of England: a tag sequence of one wide character and six zero-width ones
  const std::string england =
      "\U0001F3F4\U000E0067\U000E0062\U000E0065\U000E006E\U000E0067\U000E007F";

  SECTION("counts columns") {
    REQUIRE(utf8_width("") == 0);
    REQUIRE(utf8_width("Kopfhörer") == 9);
    REQUIRE(utf8_width("音楽") == 4);
  }

  SECTION("does not count the zero-width characters of ordinary text") {
    REQUIRE(utf8_width("é") == 1);
    REQUIRE(utf8_width("soft­hyphen") == 10);
    REQUIRE(utf8_width("‎abc") == 3);
    REQUIRE(utf8_width("한") == 2);
    REQUIRE(utf8_width("\U0001F468‍\U0001F469‍\U0001F467") == 6);
    REQUIRE(utf8_width(england + " England") == 10);
  }

  SECTION("counts the zero-width characters there are no columns for") {
    REQUIRE(utf8_width("W" + repeat("҉", 400)) == 400);
    REQUIRE(utf8_width(repeat("​", 400)) == 400);
    REQUIRE(utf8_width(repeat("W" + repeat("҉", 8), 20)) == 160);
    REQUIRE(utf8_width(england) == 6);
  }
}

TEST_CASE("utf8_truncate", "[utf8_string]") {
  SECTION("cuts at the given number of columns") {
    REQUIRE(truncated("short", 20) == "short");
    REQUIRE(truncated(repeat("W", 20), 20) == repeat("W", 20));
    REQUIRE(truncated(repeat("W", 401), 20) == repeat("W", 19) + ".");
    REQUIRE(truncated(repeat("音", 401), 20) == repeat("音", 9) + ".");
  }

  SECTION("keeps the zero-width characters of ordinary text") {
    REQUIRE(truncated(repeat("é", 20), 20) == repeat("é", 20));
    REQUIRE(truncated(repeat("é", 401), 20) == repeat("é", 19) + ".");
  }

  SECTION("does not put the ellipsis after whitespace") {
    REQUIRE(truncated("ab cdef", 4) == "ab.");
  }

  SECTION("cuts leading whitespace and a first character wider than the limit") {
    REQUIRE(truncated("    abcdef", 4) == ".");
    REQUIRE(truncated("\t 　abcdef", 3) == ".");
    REQUIRE(truncated("Ｗabc", 1) == ".");
    REQUIRE(truncated("Ｗabc", 2) == ".");
    for (size_t max_len = 1; max_len <= 20; ++max_len) {
      REQUIRE(utf8_width(truncated(std::string(20, ' ') + std::string(300, 'W'), max_len)) <=
              max_len);
    }
  }

  SECTION("does not let any number of zero-width characters through") {
    for (const char* zero_width : {"҉", "​", "­", "ᅠ", "؀"}) {
      const std::string zw = zero_width;
      REQUIRE(truncated("W" + repeat(zw, 400), 20) == "W" + repeat(zw, 19) + ".");
      REQUIRE(truncated(repeat(zw, 400) + "W", 20) == repeat(zw, 19) + ".");
      REQUIRE(truncated(repeat("W", 20) + repeat(zw, 400), 20) == repeat("W", 19) + ".");
      REQUIRE(truncated(repeat("W" + repeat(zw, 8), 20), 20) ==
              repeat("W" + repeat(zw, 8), 2) + "W" + repeat(zw, 3) + ".");
    }
  }
}
