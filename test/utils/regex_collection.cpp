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
