#include "util/tooltip.hpp"

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <string>

using waybar::util::limit_tooltip_markup;
using waybar::util::limit_tooltip_text;

namespace {

std::string repeat(const std::string& str, size_t count) {
  std::string result;
  for (size_t i = 0; i < count; ++i) {
    result += str;
  }
  return result;
}

std::string text(std::string str) {
  limit_tooltip_text(str);
  return str;
}

std::string markup(std::string str) {
  limit_tooltip_markup(str);
  return str;
}

}  // namespace

TEST_CASE("limit_tooltip_text", "[tooltip]") {
  SECTION("keeps text that GTK can wrap") {
    for (const std::string& str :
         {std::string(), std::string("Waybar"), repeat("word ", 800), repeat("中", 1000),
          repeat("a line\n", 127), repeat("https://example.org/path-", 100)}) {
      std::string copy = str;
      REQUIRE_FALSE(limit_tooltip_text(copy));
      REQUIRE(copy == str);
    }
  }

  SECTION("cuts a word that is too long for a line") {
    REQUIRE(text(repeat("W", 128)) == repeat("W", 128));
    REQUIRE(text(repeat("W", 129)) == repeat("W", 128) + "...");
    REQUIRE(text(repeat("W", 131)) == repeat("W", 128) + "...");
    REQUIRE(text("a title " + repeat("é", 3000) + " - app") ==
            "a title " + repeat("é", 128) + "...");
    // Spaces are not wrapped either
    REQUIRE(text(repeat(" ", 3000)).size() == 131);
    REQUIRE(text(repeat("\u00a0", 3000)) == repeat("\u00a0", 128) + "...");
  }

  SECTION("cuts after too many lines") {
    REQUIRE(text(repeat("\n", 3000)) == repeat("\n", 128) + "...");
    REQUIRE(text(repeat("a\r\n", 3000)) == repeat("a\r\n", 128) + "...");
    REQUIRE(text(repeat("a ", 3000)) == repeat("a ", 128) + "...");
    REQUIRE(text(repeat("a ", 3000)) == repeat("a ", 128) + "...");
  }

  SECTION("cuts too much text") {
    REQUIRE(text(repeat("W ", 2048)) == repeat("W ", 2048));
    REQUIRE(text(repeat("W ", 100000)) == repeat("W ", 2048) + "...");
  }

  SECTION("reads up to the first NUL, like GTK") {
    REQUIRE(text(repeat("W", 3000) + std::string(1, '\0') + "\xff") == repeat("W", 128) + "...");
  }

  SECTION("counts what precedes the first invalid byte, which is what Pango draws") {
    REQUIRE(text(repeat("W", 3000) + "\xff") == repeat("W", 128) + "...");
    const std::string str = repeat("W", 100) + "\xff" + repeat("W", 3000);
    REQUIRE(text(str) == str);
  }
}

TEST_CASE("limit_tooltip_markup", "[tooltip]") {
  SECTION("counts the text, not the markup") {
    for (const std::string& str :
         {std::string("<b>Waybar</b>"), repeat("&amp;", 128),
          "<span font_family=\"" + repeat("W", 200) + "\">a</span>", repeat("<b>word</b> ", 800),
          repeat("<a href=\"https://example.org\">link</a> ", 800)}) {
      REQUIRE(markup(str) == str);
    }
  }

  SECTION("drops the markup of a tooltip that is cut") {
    REQUIRE(markup("<b>" + repeat("&lt;", 3000) + "</b>") == repeat("&lt;", 128) + "...");
    REQUIRE(markup(repeat("<i>a</i>\n", 3000)) == repeat("a\n", 128) + "...");
    REQUIRE(markup("<a href=\"\">" + repeat("W", 3000) + "</a>") == repeat("W", 128) + "...");
    REQUIRE(markup(repeat("W", 3000) + std::string(1, '\0') + "<") == repeat("W", 128) + "...");
  }

  SECTION("leaves alone what GTK cannot parse") {
    const std::string str = "<b>" + repeat("W", 3000);
    REQUIRE(markup(str) == str);
  }
}
