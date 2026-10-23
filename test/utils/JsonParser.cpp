#include "util/json.hpp"

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

TEST_CASE("Simple json", "[json]") {
  SECTION("Parse simple json") {
    std::string stringToTest = R"({"number": 5, "string": "test"})";
    waybar::util::JsonParser parser;
    Json::Value jsonValue = parser.parse(stringToTest);
    REQUIRE(jsonValue["number"].asInt() == 5);
    REQUIRE(jsonValue["string"].asString() == "test");
  }
}

TEST_CASE("Json with unicode", "[json]") {
  SECTION("Parse json with unicode") {
    std::string stringToTest = R"({"test": "\xab"})";
    waybar::util::JsonParser parser;
    Json::Value jsonValue = parser.parse(stringToTest);
    // compare with "\u00ab" because "\xab" is replaced with "\u00ab" in the parser
    REQUIRE(jsonValue["test"].asString() == "\u00ab");
  }
}

TEST_CASE("Json with emoji", "[json]") {
  SECTION("Parse json with emoji") {
    std::string stringToTest = R"({"test": "😊"})";
    waybar::util::JsonParser parser;
    Json::Value jsonValue = parser.parse(stringToTest);
    REQUIRE(jsonValue["test"].asString() == "😊");
  }
}

TEST_CASE("Json with chinese characters", "[json]") {
  SECTION("Parse json with chinese characters") {
    std::string stringToTest = R"({"test": "你好"})";
    waybar::util::JsonParser parser;
    Json::Value jsonValue = parser.parse(stringToTest);
    REQUIRE(jsonValue["test"].asString() == "你好");
  }
}