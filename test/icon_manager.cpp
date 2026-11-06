#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <json/json.h>

#include <sstream>

#include "modules/sni/icon_manager.hpp"

namespace {

Json::Value parse(const std::string& json) {
  Json::Value value;
  Json::CharReaderBuilder builder;
  std::string errors;
  std::istringstream stream(json);
  REQUIRE(Json::parseFromStream(builder, stream, &value, &errors));
  return value;
}

}  // namespace

// IconManager is a singleton, so every key below is unique to its test case.

TEST_CASE("exact icon keys match only the same id", "[icon_manager]") {
  IconManager::instance().setIconsConfig(parse(R"({"exact-app": "exact.png"})"));

  REQUIRE(IconManager::instance().getIconForApp("exact-app") == "exact.png");
  REQUIRE(IconManager::instance().getIconForApp("exact-app-1234").empty());
  REQUIRE(IconManager::instance().getIconForApp("other").empty());
}

TEST_CASE("wildcard icon keys match ids that change between launches", "[icon_manager]") {
  IconManager::instance().setIconsConfig(
      parse(R"({"glob-client-*": "glob.png", "glob-single-?": "single.png"})"));

  SECTION("star matches any suffix") {
    REQUIRE(IconManager::instance().getIconForApp("glob-client-212927") == "glob.png");
    REQUIRE(IconManager::instance().getIconForApp("glob-client-") == "glob.png");
  }
  SECTION("question mark matches exactly one character") {
    REQUIRE(IconManager::instance().getIconForApp("glob-single-7") == "single.png");
    REQUIRE(IconManager::instance().getIconForApp("glob-single-77").empty());
  }
  SECTION("the whole id has to match") {
    REQUIRE(IconManager::instance().getIconForApp("glob-client").empty());
    REQUIRE(IconManager::instance().getIconForApp("my-glob-client-1").empty());
  }
}

TEST_CASE("an exact icon key wins over a wildcard", "[icon_manager]") {
  IconManager::instance().setIconsConfig(
      parse(R"({"prio-*": "pattern.png", "prio-special": "exact.png"})"));

  REQUIRE(IconManager::instance().getIconForApp("prio-special") == "exact.png");
  REQUIRE(IconManager::instance().getIconForApp("prio-other") == "pattern.png");
}

TEST_CASE("wildcard keys can hide apps", "[icon_manager]") {
  IconManager::instance().setIconsConfig(parse(R"({"hide-exact": false, "hide-glob-*": false})"));

  REQUIRE(IconManager::instance().isHidden("hide-exact"));
  REQUIRE(IconManager::instance().isHidden("hide-glob-42"));
  REQUIRE_FALSE(IconManager::instance().isHidden("hide-exact-42"));
  REQUIRE_FALSE(IconManager::instance().isHidden("visible"));
}
