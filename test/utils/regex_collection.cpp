#include "util/regex_collection.hpp"

#include <fmt/args.h>
#include <fmt/format.h>

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

using waybar::util::RegexCollection;

static Json::Value rule(const char* key, const char* repr) {
  Json::Value map(Json::objectValue);
  map[key] = repr;
  return map;
}

TEST_CASE("RegexCollection formats the rule with the match results", "[regex_collection]") {
  RegexCollection collection(rule("title<.* - (.*) - VSCodium>", "codium $1"), "?");

  std::string key = "class<codium> title<main.c - proj{ - VSCodium>";
  REQUIRE(collection.get(key) == "codium proj{");
  key = "class<codium> title<main.c>";
  REQUIRE(collection.get(key) == "?");
}

TEST_CASE("RegexCollection escapes the braces of captured text", "[regex_collection]") {
  RegexCollection collection(rule("title<.* - (.*) - VSCodium>", "codium $1 {class}"), "?",
                             waybar::util::default_priority_function, true);

  std::string key = "class<codium> title<main.c - {class} proj{ }} - VSCodium>";
  std::string repr = collection.get(key);
  REQUIRE(repr == "codium {{class}} proj{{ }}}} {class}");
  REQUIRE(fmt::format(fmt::runtime(repr), fmt::arg("class", "codium")) ==
          "codium {class} proj{ }} codium");

  // The references keep the semantics of std::match_results::format
  RegexCollection references(rule("class<(\\w+)>", "$& $$ $0 $` $' $x $1 $12"), "?",
                             waybar::util::default_priority_function, true);
  key = "x{class<ab>y}";
  REQUIRE(references.get(key) == "class<ab> $ class<ab> x{{ y}} $x ab ");
}

TEST_CASE("RegexCollection keeps its answers past the cache bound", "[regex_collection]") {
  Json::Value rules(Json::objectValue);
  rules["class<firefox>"] = "ff";
  rules["title<.*youtube.*>"] = "yt";
  RegexCollection collection(rules, "{name}");

  // A window that keeps changing its title gives a new key every time
  for (std::size_t i = 0; i < 4 * RegexCollection::MAX_CACHE_ENTRIES + 1; ++i) {
    std::string key = "class<firefox> title<" + std::to_string(i) + ">";
    REQUIRE(collection.get(key) == "ff");
    std::string miss = "class<kitty> title<" + std::to_string(i) + ">";
    REQUIRE(collection.get(miss) == "{name}");
  }

  std::string youtube = "class<chromium> title<Cats - YouTube>";
  REQUIRE(collection.get(youtube) == "yt");
  REQUIRE(collection.get(youtube) == "yt");
}

TEST_CASE("RegexCollection skips a rule that backtracks too much", "[regex_collection]") {
  RegexCollection collection(rule("title<(.*) - (.*) - (.*) - Visual Studio Code>", "code $1"),
                             "default");

  std::string title = "title<a.cpp - src - work - Visual Studio Code>";
  REQUIRE(collection.get(title) == "code a.cpp");

  std::string long_title = "title<";
  for (int i = 0; i < 1000; ++i) {
    long_title += "a - ";
  }
  long_title += ">";
  bool matched_any = false;
  REQUIRE(collection.get(long_title, matched_any) == "default");
  REQUIRE_FALSE(matched_any);
}
