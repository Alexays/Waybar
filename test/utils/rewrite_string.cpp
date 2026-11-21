#include "util/rewrite_string.hpp"

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <string>

using waybar::util::rewriteString;

static std::string repeat(const std::string& text, int count) {
  std::string res;
  for (int i = 0; i < count; ++i) {
    res += text;
  }
  return res;
}

static Json::Value rules() {
  Json::Value rules(Json::objectValue);
  rules["(.*) - Mozilla Firefox"] = "F $1";
  rules["(.*) - (.*) - (.*) - Visual Studio Code"] = "$2 ($3): $1";
  return rules;
}

TEST_CASE("rewriteString applies the matching rules", "[rewrite_string]") {
  REQUIRE(rewriteString("Waybar - Mozilla Firefox", rules()) == "F Waybar");
  REQUIRE(rewriteString("a.cpp - src - work - Visual Studio Code", rules()) == "src (work): a.cpp");
  REQUIRE(rewriteString("foot", rules()) == "foot");

  const auto long_title = repeat("a_-_", 1000);
  REQUIRE(rewriteString(long_title + " - Mozilla Firefox", rules()) == "F " + long_title);
}

TEST_CASE("rewriteString skips a rule that backtracks too much", "[rewrite_string]") {
  const auto long_title = repeat("a - ", 1000);
  REQUIRE(rewriteString(long_title, rules()) == long_title);
  REQUIRE(rewriteString(long_title + "b - Mozilla Firefox", rules()) == "F " + long_title + "b");
}
