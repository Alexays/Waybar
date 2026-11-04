#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <json/json.h>

#include "util/command.hpp"
#include "util/custom_source.hpp"

namespace {

using waybar::util::CustomSource;
using res = waybar::util::command::res;

// A config with no exec/signal/interval starts no worker thread, so the source is
// inert and its parsers can be driven directly with an explicit result.
CustomSource make(const Json::Value& config = Json::Value{}) {
  return CustomSource(config, "", [] {});
}

Json::Value jsonReturnType() {
  Json::Value config;
  config["return-type"] = "json";
  return config;
}

}  // namespace

TEST_CASE("CustomSource raw parsing fills text, tooltip and classes", "[custom_source]") {
  auto source = make();
  auto out = source.parse(res{.exit_code = 0, .out = "50\na tooltip\nwarning"});
  REQUIRE(out.ok);
  REQUIRE(out.text == "50");
  REQUIRE(out.tooltip == "a tooltip");
  REQUIRE(out.classes == std::vector<std::string>{"warning"});
  REQUIRE_FALSE(out.percentage.has_value());  // raw mode never derives a number
}

TEST_CASE("CustomSource raw tooltip defaults to the first line", "[custom_source]") {
  auto out = make().parse(res{.exit_code = 0, .out = "100.000000"});
  REQUIRE(out.ok);
  REQUIRE(out.text == "100.000000");
  REQUIRE(out.tooltip == "100.000000");
}

TEST_CASE("CustomSource is stale (never 0) on failure or empty output", "[custom_source]") {
  // Non-zero exit -> stale, not a value.
  auto failed = make().parse(res{.exit_code = 1, .out = "50"});
  REQUIRE_FALSE(failed.ok);
  REQUIRE(failed.text.empty());
  // Empty output -> stale.
  auto empty = make().parse(res{.exit_code = 0, .out = ""});
  REQUIRE_FALSE(empty.ok);

  auto failedJson =
      make(jsonReturnType()).parse(res{.exit_code = 1, .out = R"({"percentage":50})"});
  REQUIRE_FALSE(failedJson.ok);
}

TEST_CASE("CustomSource json parsing reads percentage without clamping", "[custom_source]") {
  auto source = make(jsonReturnType());
  auto out = source.parse(res{.exit_code = 0, .out = R"({"text":"hi","percentage":150})"});
  REQUIRE(out.ok);
  REQUIRE(out.text == "hi");
  REQUIRE(out.percentage == 150);  // not clamped to 100
}

TEST_CASE("CustomSource json percentage is nullopt when absent, never 0", "[custom_source]") {
  auto source = make(jsonReturnType());
  auto out = source.parse(res{.exit_code = 0, .out = R"({"text":"hi"})"});
  REQUIRE(out.ok);
  REQUIRE_FALSE(out.percentage.has_value());
}

TEST_CASE("CustomSource json parses a class array", "[custom_source]") {
  auto source = make(jsonReturnType());
  auto out = source.parse(res{.exit_code = 0, .out = R"({"class":["a","b"]})"});
  REQUIRE(out.ok);
  REQUIRE(out.classes == std::vector<std::string>{"a", "b"});
}
